/** 
  ******************************************************************************
  *  @file   main.c
  *  @brief  Demo application for the Actinium framework. The kernel part.
  *****************************************************************************/

#include <stdint.h>
#include <stdalign.h>
#include <stdnoreturn.h>
#include "actinium.h"
#include "msg_data.h"
#include "serial.h"
#include "mtimer.h"

void __assert_func(const char *f, int line, const char *fn, const char *expr) {
    serial_out("assert:\r\n");
    serial_out(expr);
    for(;;);
}

/* required by the framework */
void ac_actor_error(struct ac_actor_t* actor) {
    serial_out("task crash, restart\r\n");
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

struct mg_context_t g_mg_context;
struct ac_context_t g_ac_context; 

static struct ac_channel_t g_chan[2];

struct ac_channel_t* ac_channel_validate(
    struct ac_actor_t* actor, 
    unsigned int handle,
    bool is_write
) {
    const size_t max_id = sizeof(g_chan) / sizeof(g_chan[0]);
    return (handle < max_id) ? &g_chan[handle] : 0;
}

extern void mie_init(void);

int main(void) {
    ac_context_init();

    /* 
     * Both actors share the same priority so there's only one stack.
     */
    static alignas(1024) uint8_t stack0[1024];
    ac_context_stack_set(1, sizeof(stack0), stack0);

    static alignas(sizeof(struct example_msg_t)) struct example_msg_t g_storage[3];
    ac_channel_init_ex(&g_chan[0], sizeof(g_storage), g_storage, sizeof(g_storage[0]), 1);
    ac_channel_init(&g_chan[1], 1);

    static struct ac_actor_t g_receiver;
    ac_actor_init(&g_receiver, 1, descr_by_id(0));
    ac_actor_allow(&g_receiver, 32, (void*)SERIAL_BASE, AC_ATTR_DEV);

    static struct ac_actor_t g_sender;
    ac_actor_init(&g_sender, 1, descr_by_id(1));
    ac_actor_allow(&g_sender, 32, (void*)SERIAL_BASE, AC_ATTR_DEV);

    mie_init();
    mtimer_setup();
    ac_kernel_start(); /* Does not return. */

    return 0;
}

