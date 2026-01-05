/*
 *  @file   alloc_send.c
 *  @brief  Demo for hosted HAL.
 */

#include <stdio.h>
#include <stdalign.h>
#include <setjmp.h>
#include <assert.h>
#include "ac_core.h"          /* kernel API */
#include "usr/c/actinium.h"   /* user API */

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
    uint32_t foo[9];
};

_Static_assert(sizeof(struct demo_msg_t) == 64, "wrong msg size");

void ac_actor_error(struct ac_actor_t* actor) {
    ac_actor_restart(actor);
}

uint32_t actor1(void* arg) {
    struct demo_msg_t* msg = arg;
    AC_ACTOR_START;
    AC_AWAIT(ac_subscribe_to(1));
    assert(msg->foo[0] == 0xc0cac01a);
    ac_free();
    AC_AWAIT(ac_sleep_for(1));
    AC_ACTOR_END;
}

uint32_t actor2(void* arg) {
    struct demo_msg_t* msg = arg;
    AC_ACTOR_START
    msg = ac_try_pop(0);
    assert(msg != 0);
    msg->foo[0] = 0xc0cac01a;
    ac_push(1);
    AC_AWAIT(ac_sleep_for(1));
    AC_ACTOR_END;
}

uint32_t actor3(void* arg) {
    struct demo_msg_t* msg = arg;
    AC_ACTOR_START;
    msg = ac_try_pop(0);
    assert(msg != 0);
    msg->foo[0] = 0xcafebabe;
    ac_push(1);
    msg = ac_try_pop(0);
    assert(msg != 0);
    msg->foo[0] = 0xc0cac01a;
    ac_push(1);
    msg = ac_try_pop(1);
    assert(msg != 0);
    assert(msg->foo[0] == 0xcafebabe);
    msg = ac_try_pop(1);
    assert(msg != 0);
    assert(msg->foo[0] == 0xc0cac01a);
    ac_free();
    AC_AWAIT(ac_sleep_for(1));
    AC_ACTOR_END;
}

int main(void) {
    ac_context_init();
    static uint8_t stack0[512];
    static uint8_t stack1[512];
    ac_context_stack_set(0, sizeof(stack0), stack0);
    ac_context_stack_set(1, sizeof(stack1), stack1);

    static alignas(sizeof(struct demo_msg_t)) struct demo_msg_t g_storage[2];
    ac_channel_init_ex(&g_chan[0], sizeof(g_storage), g_storage, sizeof(g_storage[0]), 1);
    ac_channel_init(&g_chan[1], 1);
    
    static struct ac_actor_t g_receiver;
    struct ac_actor_descr_t receiver_descr = { (uintptr_t) actor1, 32, 0, 0 };
    ac_actor_init(&g_receiver, 0, &receiver_descr);

    static struct ac_actor_t g_sender;
    struct ac_actor_descr_t sender_descr = { (uintptr_t) actor2, 32, 0, 0 };
    ac_actor_init(&g_sender, 0, &sender_descr);

    static struct ac_actor_t g_actor;
    struct ac_actor_descr_t descr = { (uintptr_t) actor3, 32, 0, 0 };
    ac_actor_init(&g_actor, 1, &descr);

    ac_port_swi_handler();
    return 0;
}

