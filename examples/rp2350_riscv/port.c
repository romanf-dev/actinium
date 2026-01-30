/*
 * @file  port.c
 * @brief Porting layer for Hazard3 interrupt controller.
 *
 * Design notes:
 * Since it is impossible to send arbitrary interrupts to another CPU the
 * process is two-staged: 
 * - Target irq is saved inside a per-CPU bitmask.
 * - Inter-processor doorbell interrupt is requested.
 * - The doorbell handler on the target CPU translates bits back to the vectors.
 * - Local interrupt is requested on the target CPU inside a doorbell handler.
 *
 * PMP region 7 is used to override hardwired regions 8+. It disables access
 * to the whole address space unless some lower numbered regions are also 
 * match the address.
 *
 * Mtimer and MSoftirq are not used.
 */

#include <stdint.h>
#include <hardware/structs/sio.h>
#include "mtimer.h"
#include "actinium.h"

#define meiea       0x0be0
#define meifa       0x0be2
#define meipra      0x0be3
#define meinext     0x0be4
#define meicontext  0x0be5

#define _csrr(csr, data) asm volatile ("csrr %0, " #csr : "=r" (data))
#define csrr(csr, data) _csrr(csr, data)
#define _csrw(csr, data) asm volatile ("csrw " #csr ", %0" : : "r" (data))
#define csrw(csr, data) _csrw(csr, data)
#define _csrrsi(csr, rd, m) asm volatile ("csrrsi %0, " #csr ", %1": "=r" (rd) : "i" (m))
#define csrrsi(csr, rd, bits) _csrrsi(csr, rd, bits)
#define _csrrs(csr, rd, m) asm volatile ("csrrs %0, " #csr ", %1": "=r" (rd) : "r" (m))
#define csrrs(csr, rd, bits) _csrrs(csr, rd, bits)

enum {
    MEIPRA_BIT_PER_PRIO = 4,
    MEIPRA_PRIO_PER_WINDOW = 4,
    MEIPRA_PRIO_MASK = (1 << MEIPRA_BIT_PER_PRIO) - 1,
    MEIFA_IRQ_PER_WINDOW = 16,
    MEIEA_IRQ_PER_WINDOW = 16,
    IRQ_DOORBELL = 26,
    IRQ_MTIMER = 29,
    MEI_MEIE = 1 << 11,
    CLK_PER_TICK = 12000,
    IRQ_BITMAP_UINTS_COUNT = 2, /* Number of uints to hold all irq vectors.*/
};

static atomic_uint g_ipi_request[IRQ_BITMAP_UINTS_COUNT][MG_CPU_MAX];

static inline void hazard3_local_irq_enable(unsigned vec) {
    uint32_t enabled_irqs = 0;
    const uint32_t window = vec / MEIEA_IRQ_PER_WINDOW;
    const uint32_t offset = vec % MEIEA_IRQ_PER_WINDOW;
    csrrs(meiea, enabled_irqs, window);
    enabled_irqs |= UINT32_C(1) << (offset + 16);
    csrw(meiea, enabled_irqs | window);
}

static inline void hazard3_local_irq_request(unsigned vect) {
    const uint32_t window = vect / MEIFA_IRQ_PER_WINDOW;
    const uint32_t offset = vect % MEIFA_IRQ_PER_WINDOW;
    const uint32_t mask = UINT32_C(1) << (offset);
    csrw(meifa, (mask << 16) | window);
}

void pic_interrupt_request(unsigned cpu, unsigned vect) {
    if (cpu != mg_cpu_this()) {
        const unsigned dword = vect / 32u;
        const unsigned bit = vect % 32u;
        (void) atomic_fetch_or(&g_ipi_request[dword][cpu], 1u << bit);
        sio_hw->doorbell_out_set = 1;
    } else {
        hazard3_local_irq_request(vect);
    }
}

static inline void hazard3_doorbell_handler(void) {
    const unsigned cpu = mg_cpu_this();
    sio_hw->doorbell_in_clr = 1;

    for (unsigned i = 0; i < IRQ_BITMAP_UINTS_COUNT; ++i) {
        unsigned req = atomic_exchange(&g_ipi_request[i][cpu], 0);

        while (req) {
            const unsigned bit = 31u - mg_port_clz(req);
            const unsigned vect = i * 32u + bit;
            req &= ~(1u << bit);
            hazard3_local_irq_request(vect);
        }
    }
}

static inline struct ac_port_frame_t* hazard3_local_irq_handler(
    struct ac_port_frame_t* frame,
    uint32_t offset
) {
    const unsigned vect = offset >> 2;
    struct ac_port_frame_t* next_frame = frame;
    
    switch (vect) {
    case IRQ_DOORBELL:
        hazard3_doorbell_handler();
        break;
    case IRQ_MTIMER:
        const uint64_t current = riscv_timer_get_mtime();
        const uint64_t next = current + CLK_PER_TICK;
        riscv_timer_set_mtimecmp(next);
        ac_context_tick();
        break;
    default:
        next_frame = _ac_intr_handler(vect, frame);
        /*TODO: call user ISR if vector isn't associated with actors. */
    };

    return next_frame;
}

//
// There are two kinds in interrupts: those who is fully handled inside the
// handler and whose handling is performed in usermode (running actors).
// If handler returns new interrupt frame it means that we have to go to 
// usermode. In this case we DO NOT want to perform priority unstacking on
// mret so the MRETEIRQ bit in MEICONTEXT is cleared.
//

struct ac_port_frame_t* ac_port_mei_handler(struct ac_port_frame_t* frame) {
    uint32_t context = 0;
    uint32_t next = 0;
    struct ac_port_frame_t* next_frame = frame;
    csrr(meicontext, context);
    csrrsi(meinext, next, 1);

    while ((next >> 31) == 0) {
        mg_critical_section_leave();
        next_frame = hazard3_local_irq_handler(frame, next);
        mg_critical_section_enter();

        if (next_frame != frame) {
            context &= ~UINT32_C(1);
            break;
        }

        csrrsi(meinext, next, 1);
    }
    
    csrw(meicontext, context);
    return next_frame;
}

//
// In case of either blocking call or exception the priority unstacking has
// to be performed. So when next frame is not the previous one it means that
// the current actor is completed by some reason, set the MRETEIRQ flag.
//

struct ac_port_frame_t* ac_port_trap_handler(
    struct ac_port_frame_t* frame,
    uint32_t mcause
) {
    mg_critical_section_leave();
    const uint32_t syscall = frame->r[REG_A0];
    struct ac_port_frame_t* const next_frame = (mcause == 8) ?
        (frame->pc += sizeof(uint32_t)), _ac_svc_handler(syscall, frame):
        ac_actor_exception();
    mg_critical_section_enter();

    if (next_frame != frame) {
        uint32_t context = 0;
        csrrsi(meicontext, context, 1);
    }

    return next_frame;
}

struct ac_port_frame_t* ac_port_mtimer_handler(struct ac_port_frame_t* prev) {
    (void) prev;
    for (;;);
    return 0;
}

struct ac_port_frame_t* ac_port_msi_handler(struct ac_port_frame_t* prev) {
    (void) prev;
    for (;;);
    return 0;
}

unsigned pic_vect2prio(unsigned vec) {
    uint32_t prio_array = 0;
    const uint32_t offset = (vec % MEIPRA_PRIO_PER_WINDOW) * MEIPRA_BIT_PER_PRIO;
    const uint32_t window = vec / MEIPRA_PRIO_PER_WINDOW;
    csrrs(meipra, prio_array, window);
    return (prio_array >> (offset + 16)) & MEIPRA_PRIO_MASK;
}

void rp2350_per_cpu_init(uint64_t actor_vects) {
    csrw(pmpaddr7, 0x3fffffff);
    csrw(pmpcfg1, 0x18000000);
    csrw(mie, MEI_MEIE);
    hazard3_local_irq_enable(IRQ_MTIMER);
    hazard3_local_irq_enable(IRQ_DOORBELL);

    for (unsigned i = 0; i < sizeof(uint64_t) * CHAR_BIT; ++i) {
        if (actor_vects & (UINT64_C(1) << i)) {
            hazard3_local_irq_enable(i);
        }
    }

    riscv_timer_set_mtimecmp(CLK_PER_TICK);
}

