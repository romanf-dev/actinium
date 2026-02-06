/*
 * @file  main.c
 * @brief Demo application for RP2350 in ARM mode.
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
#include "RP2350.h"
#include "led_msg.h"
#include "actinium.h"

enum {
    SPAREIRQ_IRQ_0 = 46, // Spare irq vector. See 3.8.6.1.2 in the datasheet.
    SYSTICK_VAL = 12000, // Clocks at boot without PLL setup.
    EXTEXCLALL = 1 << 29,// See ACTLR bits in 3.7.5 in the datasheet.
    UINT_MSB = sizeof(unsigned int) * CHAR_BIT - 1,
    STACK_SZ = 1024,
    USER_PRIO_BASE = 2,  // Priorities 0 and 1 are reserved.
};

static noreturn void panic(void) {
    __disable_irq();
    for (;;);
}

void __assert_func(const char *file, int ln, const char *fn, const char *expr) {
    panic();
}

void hard_fault_isr(void) {
    panic();
}

void* ac_intr_handler(uint32_t vect, void* frame) {
    return _ac_intr_handler(vect, frame);
}

void* ac_svc_handler(uint32_t arg, void* frame) {
    return _ac_svc_handler(arg, frame);
}

void* ac_trap_handler(uint32_t id) {
    return ac_actor_exception();
}

static atomic_uint g_ipi_request[MG_CPU_MAX];

unsigned int mg_cpu_this(void) {
    return SIO->CPUID & 1;
}

void pic_interrupt_request(unsigned int cpu, unsigned int vect) {
    if (cpu != mg_cpu_this()) {
        const unsigned int prio = pic_vect2prio(vect) - USER_PRIO_BASE;
        (void) atomic_fetch_or(&g_ipi_request[cpu], 1U << prio);
        SIO->DOORBELL_OUT_SET = 1;
    } else {
        NVIC->STIR = vect;
    }
}

void doorbell_isr(void) {   
    SIO->DOORBELL_IN_CLR = 1;
    const unsigned int cpu = mg_cpu_this();
    unsigned int prev = atomic_exchange(&g_ipi_request[cpu], 0);

    while (prev) {
        const unsigned int prio = UINT_MSB - mg_port_clz(prev);
        const unsigned int vect = prio + SPAREIRQ_IRQ_0;
        prev &= ~(1U << prio);
        NVIC->STIR = vect;
    }
}

void systick_isr(void) {
    ac_context_tick();
}

static inline void per_cpu_init(void) {
    SCnSCB->ACTLR |= EXTEXCLALL;
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
    NVIC_SetPriorityGrouping(3);
    NVIC_SetPriority(SIO_IRQ_BELL_IRQn, 1);
    NVIC_SetPriority(SysTick_IRQn, 1);
    NVIC_SetPriority(SVCall_IRQn, 1);
    NVIC_SetPriority(SPAREIRQ_IRQ_0, USER_PRIO_BASE);
    NVIC_EnableIRQ(SIO_IRQ_BELL_IRQn);
    NVIC_EnableIRQ(SPAREIRQ_IRQ_0);
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

extern void core1_start(void (*fn)(void));

noreturn void core1_main(void) {
    ac_context_init();
    per_cpu_init();
    static alignas(32) uint8_t stack[STACK_SZ];
    ac_context_stack_set(2, sizeof(stack), stack);
    static struct ac_actor_t g_sender;
    ac_actor_init(&g_sender, SPAREIRQ_IRQ_0, descr_by_id(1));

    SysTick->LOAD = SYSTICK_VAL;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    __enable_irq();
    ac_kernel_start();
}

noreturn int main(void) {
    IO_BANK0->GPIO25_CTRL = 5;
    PADS_BANK0->GPIO25 = 0x34;
    SIO->GPIO_OE_SET = 1U << 25;
    SIO->GPIO_OUT_CLR = 1U << 25;

    ac_context_init();
    per_cpu_init();
    
    static alignas(32) uint8_t stack[STACK_SZ];
    ac_context_stack_set(2, sizeof(stack), stack);

    static alignas(32) struct led_msg_t g_storage[3];
    ac_channel_init_ex(&g_chan[0], sizeof(g_storage), g_storage, sizeof(g_storage[0]));
    ac_channel_init(&g_chan[1]);

    static struct ac_actor_t g_handler;
    ac_actor_init(&g_handler, SPAREIRQ_IRQ_0, descr_by_id(0));
    ac_actor_allow(&g_handler, 256, (void*)0xd0000000u, AC_ATTR_DEV);

    core1_start(core1_main);

    SysTick->LOAD = SYSTICK_VAL;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    __enable_irq();
    ac_kernel_start();
}

