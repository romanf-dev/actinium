/*
 *  @file  main.h
 *  @brief Demo application for RP2350 in RISC-V mode.
 *
 *  Design notes:
 *  Since it is impossible to send arbitrary interrupts to another CPU the
 *  process is two-staged: 
 *  - Target irq is translated into the priority and is saved inside a
 *    per-CPU bitmask.
 *  - Inter-processor doorbell interrupt is requested.
 *  - The doorbell handler on the target CPU translates priority from the
 *    bitmask back to the interrupt vector.
 *  - Local interrupt is requested on the target CPU inside a doorbell handler.
 *  Currently the mapping is linear: 
 *  0->N priorities is mapped to SPARE_IRQ_MIN->N vectors.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <stdalign.h>
#include <hardware/structs/sio.h>
#include <hardware/structs/iobank0.h>
#include <hardware/structs/padsbank0.h>
#include <hardware/structs/accessctrl.h>
#include "mtimer.h"
#include "led_msg.h"
#include "actinium.h"

#define _csrr(csr, data) asm volatile ("csrr %0, " #csr : "=r" (data))
#define csrr(csr, data) _csrr(csr, data)
#define _csrw(csr, data) asm volatile ("csrw " #csr ", %0" : : "r" (data))
#define csrw(csr, data) _csrw(csr, data)
#define _csrrsi(csr, rd, m) asm volatile ("csrrsi %0, " #csr ", %1": "=r" (rd) : "i" (m))
#define csrrsi(csr, rd, bits) _csrrsi(csr, rd, bits)

#define irq_disable() asm volatile ("csrc mstatus, 8")
#define irq_enable() asm volatile ("csrs mstatus, 8")

#define meiea       0x0be0
#define meifa       0x0be2
#define meipra      0x0be3
#define meinext     0x0be4
#define meicontext  0x0be5

enum {
    SPARE_IRQ_MIN = 48, /* Use 48-51 range as it is within a single meiea CSR.*/
    SPARE_IRQ_MAX = 51,
    MEIPRA_BIT_PER_PRIO = 4,
    MEIPRA_PRIO_PER_WINDOW = 4,
    MEIPRA_PRIO_MASK = (1 << MEIPRA_BIT_PER_PRIO) - 1,
    MEIFA_IRQ_PER_WINDOW = 16,
    MEIEA_IRQ_PER_WINDOW = 16,
    GPIO_LED = 25,
    DOORBELL_IRQ = 26,
    MTIMER_IRQ = 29,
    MEI_MEIE = 1 << 11,
    CLK_PER_TICK = 12000,
    STACK_SZ = 1024,
};

static noreturn void panic(void) {
    irq_disable();
    sio_hw->gpio_set = 1u << GPIO_LED;
    for (;;);
}

void __assert_func(const char *file, int ln, const char *fn, const char *expr) {
    (void) file;
    (void) ln;
    (void) fn;
    (void) expr;
    panic();
}

unsigned pic_vect2prio(unsigned vec) {
    uint32_t prio_array = 0;
    const unsigned offset = (vec % MEIPRA_PRIO_PER_WINDOW) * MEIPRA_BIT_PER_PRIO;
    assert((vec >= SPARE_IRQ_MIN) && (vec <= SPARE_IRQ_MAX));
    csrrsi(meipra, prio_array, SPARE_IRQ_MIN / MEIPRA_PRIO_PER_WINDOW);
    return (prio_array >> (offset + 16)) & MEIPRA_PRIO_MASK;
}

static atomic_uint g_ipi_request[MG_CPU_MAX];

static inline void request_local_interrupt(unsigned vect) {
    assert((vect >= SPARE_IRQ_MIN) && (vect <= SPARE_IRQ_MAX));
    const unsigned window = vect / MEIFA_IRQ_PER_WINDOW;
    const unsigned mask = 1u << (vect % MEIFA_IRQ_PER_WINDOW);
    csrw(meifa, (mask << 16) | window);
}

void pic_interrupt_request(unsigned cpu, unsigned vect) {
    if (cpu != mg_cpu_this()) {
        const unsigned prio = pic_vect2prio(vect);
        (void) atomic_fetch_or(&g_ipi_request[cpu], 1u << prio);
        sio_hw->doorbell_out_set = 1;
    } else {
        request_local_interrupt(vect);
    }
}

static inline void doorbell_handler(void) {   
    sio_hw->doorbell_in_clr = 1;
    const unsigned cpu = mg_cpu_this();
    unsigned req = atomic_exchange(&g_ipi_request[cpu], 0);

    while (req) {
        const unsigned prio = sizeof(unsigned) * CHAR_BIT - 1 - mg_port_clz(req);
        const unsigned vect = prio + SPARE_IRQ_MIN;
        req &= ~(1u << prio);
        request_local_interrupt(vect);
    }
}

static inline struct ac_port_frame_t* irq_handler(
    struct ac_port_frame_t* frame,
    uint32_t offset
) {
    const unsigned vect = offset >> 2;
    struct ac_port_frame_t* next_frame = frame;
    
    switch (vect) {
    case DOORBELL_IRQ:
        doorbell_handler();
        break;
    case MTIMER_IRQ:
        const uint64_t current = riscv_timer_get_mtime();
        const uint64_t next = current + CLK_PER_TICK;
        riscv_timer_set_mtimecmp(next);
        ac_context_tick();
        break;
    default:
        next_frame = _ac_intr_handler(vect, frame);
    };

    return next_frame;
}

static inline void enable_vect(unsigned int vec) {
    uint32_t enabled_irqs = 0;
    const uint32_t window = vec / MEIEA_IRQ_PER_WINDOW;
    csrrsi(meiea, enabled_irqs, window);
    enabled_irqs |= 1u << ((vec % MEIEA_IRQ_PER_WINDOW) + 16);
    csrw(meiea, enabled_irqs | window);
}

//
// PMP region 7 is used to override hardwired regions 8+. It disables access
// to the whole address space unless some lower numbered regions are also 
// match the address.
//

static inline void per_cpu_init(void) {
    csrw(pmpaddr7, 0x3fffffff);
    csrw(pmpcfg1, 0x18000000);
    csrw(mie, MEI_MEIE);
    enable_vect(MTIMER_IRQ);
    enable_vect(DOORBELL_IRQ);
    enable_vect(SPARE_IRQ_MIN);
    riscv_timer_set_mtimecmp(CLK_PER_TICK);
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
        next_frame = irq_handler(frame, next);
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

struct mg_context_t g_mg_context;
struct ac_context_t g_ac_context; 
static struct ac_channel_t g_chan[2];

struct ac_channel_t* ac_channel_validate(
    struct ac_actor_t* actor, 
    unsigned int handle,
    bool is_write
) {
    (void) actor;
    (void) is_write;
    const size_t max_id = sizeof(g_chan) / sizeof(g_chan[0]);
    return (handle < max_id) ? &g_chan[handle] : 0;
}

void ac_actor_error(struct ac_actor_t* actor) {
    ac_actor_restart(actor);
}

static struct ac_actor_descr_t* descr_by_id(unsigned int task_id) {
    extern const uintptr_t _ac_task_mem_map[]; /* From the linker script. */
    const uintptr_t* const config = (const uintptr_t*) &_ac_task_mem_map;
    const size_t task_num = config[0];
    const struct {
        uintptr_t flash_addr;
        uintptr_t flash_size;
        uintptr_t sram_addr; 
        uintptr_t sram_size;
    } * const slot = (void*) (config + 1);

    _Static_assert(sizeof(*slot) == sizeof(uintptr_t) * 4, "padding");
    assert(task_id < task_num);
    static struct ac_actor_descr_t descr;

    descr.flash_addr = slot[task_id].flash_addr;
    descr.flash_size = slot[task_id].flash_size;
    descr.sram_addr = slot[task_id].sram_addr;
    descr.sram_size = slot[task_id].sram_size;
    return &descr;
}

extern void gp_init(void);
extern void core1_start(void (*fn)(void), uintptr_t mtvec, uintptr_t stack);

noreturn void core1_main(void) {
    irq_disable();
    gp_init();
    ac_context_init();
    static alignas(STACK_SZ) uint8_t stack0[STACK_SZ];
    ac_context_stack_set(0, sizeof(stack0), stack0);
    static struct ac_actor_t g_sender;
    ac_actor_init(&g_sender, SPARE_IRQ_MIN, descr_by_id(1));
    per_cpu_init();
    ac_kernel_start();
}

noreturn int main(void) {
    io_bank0_hw->io[GPIO_LED].ctrl = 5;
    pads_bank0_hw->io[GPIO_LED] = 0x34;
    sio_hw->gpio_oe_set = 1u << GPIO_LED;
    sio_hw->gpio_clr = 1u << GPIO_LED;

    ac_context_init();
    static alignas(STACK_SZ) uint8_t stack0[STACK_SZ];
    ac_context_stack_set(0, sizeof(stack0), stack0);

    static alignas(sizeof(struct led_msg_t)) struct led_msg_t g_storage[3];
    ac_channel_init_ex(&g_chan[0], sizeof(g_storage), g_storage, sizeof(g_storage[0]), 1);
    ac_channel_init(&g_chan[1], 1);

    static struct ac_actor_t g_handler;
    ac_actor_init(&g_handler, SPARE_IRQ_MIN, descr_by_id(0));
    ac_actor_allow(&g_handler, 256, (void*)0xd0000000u, AC_ATTR_DEV);

    irq_disable();
    accessctrl_hw->gpio_nsmask[0] = ~0u;
    sio_hw->mtime_ctrl &= ~1u;
    riscv_timer_set_mtime(0);
    sio_hw->mtime_ctrl |= 1 | (1 << 1);

    extern uint32_t _estack1;
    const uintptr_t core1_stack = (uintptr_t) &_estack1;
    uintptr_t mtvec;
    csrr(mtvec, mtvec);
    core1_start(core1_main, mtvec, core1_stack);
    per_cpu_init();
    ac_kernel_start();
}

