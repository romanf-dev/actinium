/** 
  ******************************************************************************
  *  @file   test0.c
  *  @brief  Demo for hosted HAL.
  *****************************************************************************/

#include <stdio.h>
#include <stdalign.h>
#include <setjmp.h>
#include "actinium.h"           /* kernel API */
#include "c_task/actinium.h"    /* user API */

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

struct demo_msg_t {
    struct ac_message_t header;
    uint32_t foo[8];
};

_Static_assert(sizeof(struct demo_msg_t) == 64, "wrong msg size");

void ac_actor_error(struct ac_actor_t* actor) {
    printf("task crash, restart\r\n");
    ac_actor_restart(actor);
}

uint32_t actor1(void* arg) {
    struct demo_msg_t* msg = arg;
    AC_ACTOR_START;
    printf("actor1 start\r\n");
    AC_AWAIT(ac_subscribe_to(1));
    printf("actor1 got a message: %08x\r\n", msg->foo[0]);
    printf("actor1: sleep\r\n");
    AC_AWAIT(ac_sleep_for(100));
    AC_ACTOR_END;
    return 0;
}

uint32_t actor2(void* arg) {
    struct demo_msg_t* msg = arg;
    AC_ACTOR_START
    printf("actor2 start\r\n");
    msg = ac_try_pop(0);
    printf("actor2: msg alloc (addr = %p)\r\n", (void*)msg);
    msg->foo[0] = 0xc0cac01a;
    ac_push(1);
    printf("actor2: sleep\r\n");
    AC_AWAIT(ac_sleep_for(100));
    AC_ACTOR_END;
    return 0;
}

int main(void) {
    /* 
     * Base address of flash and its size.
     */
    ac_context_init();

    /* 
     * Both actors share the same priority 2 so there's only one stack.
     */
    static uint8_t stack0[512];
    ac_context_stack_set(1, sizeof(stack0), stack0);

    /* 
     * Create global objects, 1 is used as unique type id, any value may be used.
    */ 
    static alignas(sizeof(struct demo_msg_t)) struct demo_msg_t g_storage[3];
    ac_channel_init_ex(&g_chan[0], sizeof(g_storage), g_storage, sizeof(g_storage[0]), 1);
    ac_channel_init(&g_chan[1], 1);
    
    static struct ac_actor_t g_receiver;
    struct ac_actor_descr_t receiver_descr = { (uintptr_t) actor1, 32, 32, 32 };
    ac_actor_init(&g_receiver, 1, &receiver_descr);

    static struct ac_actor_t g_sender;
    struct ac_actor_descr_t sender_descr = { (uintptr_t) actor2, 32, 32, 32 };
    ac_actor_init(&g_sender, 1, &sender_descr);

    ac_port_swi_handler();
    printf("exiting...\r\n");
    return 0;
}
