/*
 * @file  main.c
 * @brief Demo application for RP2350 in RISC-V mode.
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

#define irq_disable() asm volatile ("csrc mstatus, 8")
#define irq_enable() asm volatile ("csrs mstatus, 8")

extern void gp_init(void);
extern void rp2350_core1_start(void (*f)(void), uintptr_t mtvec, uintptr_t stk);
extern void rp2350_per_cpu_init(uint64_t actor_vects);

enum {
    SPARE_IRQ_MIN = 48,
    SPARE_IRQ_MAX = 51,
    GPIO_LED = 25,
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

static noreturn void core1_main(void) {
    irq_disable();
    gp_init();
    ac_context_init();

    static alignas(STACK_SZ) uint8_t stack0[STACK_SZ];
    ac_context_stack_set(0, sizeof(stack0), stack0);

    static struct ac_actor_t g_sender;
    ac_actor_init(&g_sender, SPARE_IRQ_MIN, descr_by_id(1));

    rp2350_per_cpu_init(UINT64_C(1) << SPARE_IRQ_MIN);
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

    extern void ac_port_intr_entry(void);
    extern uint32_t _estack1;

    const uintptr_t core1_stack = (uintptr_t) &_estack1;
    const uintptr_t mtvec = (uintptr_t) &ac_port_intr_entry;
    rp2350_core1_start(core1_main, mtvec, core1_stack);
    rp2350_per_cpu_init(UINT64_C(1) << SPARE_IRQ_MIN);
    ac_kernel_start();
}

