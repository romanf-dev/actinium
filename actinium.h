/*
 *  @file   actinium.h
 *  @brief  Actinium kernel implementation. Functions with underline prefix
 *  are internal.
 */

#ifndef ACTINIUM_H
#define ACTINIUM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include "magnesium.h"
#include "ac_port.h"

enum {
    AC_CALL_DELAY,
    AC_CALL_SUBSCRIBE,
    AC_CALL_TRY_POP,
    AC_CALL_PUSH,
    AC_CALL_FREE,
    AC_CALL_MAX
};

enum {  
    AC_REGION_MSG = AC_PORT_REGIONS_NUM,
    AC_REGION_USER,
    AC_REGIONS_NUM,
};

/*
 * There are two headers, one is used when a message isn't accessible from
 * usermode and another is used when some actor owns the message. The latter
 * contains only size as its first member.
 * Poisoned message is a message that is returned to a channel by exception,
 * so its data may be invalid. Uintptr type is used to simplify ABI since
 * mg_message_t only contains pointers.
 */
struct ac_message_t {
    union {
        struct mg_message_t header;
        size_t size;
    };
    uintptr_t poisoned;
};

struct ac_actor_t {
    struct mg_actor_t base;
    struct ac_port_region_t granted[AC_REGIONS_NUM];
    uintptr_t func;
    bool restart_req;
    struct ac_channel_t* msg_parent;
};

struct ac_cpu_context_t {
    struct ac_actor_t* running_actor;
    struct ac_port_region_t granted[AC_REGIONS_NUM];

    struct {
        struct ac_port_frame_t* frame;
        struct ac_actor_t* actor;
    } preempted[MG_PRIO_MAX];

    struct {
        uintptr_t top;
        size_t size;
    } stacks[MG_PRIO_MAX];
};

struct ac_context_t {
    struct ac_cpu_context_t per_cpu_data[MG_CPU_MAX];
};

struct ac_channel_t {
    struct mg_message_pool_t base;
};

_Static_assert(offsetof(struct ac_message_t, header) == 0, "non 1st member");
_Static_assert(offsetof(struct ac_actor_t, base) == 0, "non 1st member");
_Static_assert(offsetof(struct ac_channel_t, base) == 0, "non 1st member");
_Static_assert(sizeof(struct ac_message_t) == sizeof(uintptr_t) * 3, "pad");

extern noreturn void ac_kernel_start(void);
extern void* ac_intr_handler(uint32_t vect, void* frame);
extern void* ac_svc_handler(uint32_t arg, void* frame);
extern void* ac_trap_handler(uint32_t exception_id);
extern void ac_actor_error(struct ac_actor_t* src);
extern struct ac_channel_t* ac_channel_validate(
    struct ac_actor_t* caller, 
    unsigned int handle,
    bool is_write
);

extern struct ac_context_t g_ac_context;
#define AC_GET_CONTEXT() (&(g_ac_context.per_cpu_data[mg_cpu_this()]))

static inline void ac_context_init(void) {
    if (mg_cpu_this() == 0) {
        mg_context_init();
    }

    struct ac_cpu_context_t* const context = AC_GET_CONTEXT();
    ac_port_init(AC_REGIONS_NUM, context->granted);
}

static inline void ac_context_tick(void) {
    mg_context_tick();
}

static inline void ac_context_stack_set(unsigned prio, size_t sz, void* ptr) {
    struct ac_cpu_context_t* const context = AC_GET_CONTEXT();
    assert((sz & (sz - 1)) == 0);
    assert(sz > sizeof(struct ac_port_frame_t));
    context->stacks[prio].top = (uintptr_t) ptr + sz;
    context->stacks[prio].size = sz;
}

static inline void ac_channel_init_ex(
    struct ac_channel_t* chan, 
    size_t total_len,
    void* mem,
    size_t block_sz
) {
    assert((block_sz & (block_sz - 1)) == 0);
    assert((((uintptr_t)mem) & (block_sz - 1)) == 0);
    mg_queue_init(&chan->base.queue);
    chan->base.array = mem;
    chan->base.total_length = total_len;
    chan->base.block_sz = block_sz;
    chan->base.offset = 0;
    chan->base.array_space_available = (total_len != 0);
}

static inline void ac_channel_init(struct ac_channel_t* chan) {
    ac_channel_init_ex(chan, 0, 0, 0);
}

static inline void _ac_message_bind(struct ac_actor_t* actor) {
    struct ac_message_t* const msg = (void*) actor->base.mailbox;
    const bool not_bound = actor->msg_parent == 0;

    if (msg && not_bound) {
        struct ac_channel_t* const parent = (void*) msg->header.parent;
        struct ac_port_region_t* const region = &actor->granted[AC_REGION_MSG];       
        actor->msg_parent = parent;
        msg->size = parent->base.block_sz;
        ac_port_region_init(region, (uintptr_t)msg, msg->size, AC_ATTR_RW);
    }
}

static inline void _ac_message_unbind(struct ac_actor_t* actor) {
    struct ac_message_t* const msg = (void*) actor->base.mailbox;
    assert(msg != 0);
    msg->header.parent = &actor->msg_parent->base;
    actor->base.mailbox = 0;
    actor->msg_parent = 0;
    ac_port_region_init(&actor->granted[AC_REGION_MSG], 0, 0, AC_ATTR_RW);
}

static inline void _ac_message_release(struct ac_actor_t* actor, bool poisoned) {
    struct ac_message_t* const msg = (void*) actor->base.mailbox;

    if (msg) {
        _ac_message_unbind(actor);
        msg->poisoned = poisoned;
        mg_message_free(&msg->header);
    }
}

static inline void _ac_channel_push(
    struct ac_actor_t* src, 
    struct ac_channel_t* dst
) {
    struct ac_message_t* const msg = (void*) src->base.mailbox;

    if (msg) {
        msg->poisoned = 0;
        _ac_message_unbind(src);
        mg_queue_push(&dst->base.queue, &msg->header);
    }
}

struct ac_actor_descr_t {
    uintptr_t flash_addr;
    size_t flash_size;
    uintptr_t sram_addr; 
    size_t sram_size;
};

static inline void ac_actor_init(
    struct ac_actor_t* actor, 
    unsigned int vect,
    struct ac_actor_descr_t* descr
) {
    struct ac_port_region_t* regions = actor->granted;
    mg_actor_init(&actor->base, 0, vect, 0); /* Null func means usermode. */
    actor->func = descr->flash_addr;
    actor->restart_req = true;
    actor->msg_parent = 0;
    struct ac_cpu_context_t* const context = AC_GET_CONTEXT();

    ac_port_region_init(
        &regions[AC_PORT_REGION_FLASH], 
        descr->flash_addr, 
        descr->flash_size, 
        AC_ATTR_RO
    );
    ac_port_region_init(
        &regions[AC_PORT_REGION_SRAM], 
        descr->sram_addr, 
        descr->sram_size, 
        AC_ATTR_RW
    );
    const unsigned int prio = actor->base.prio;
    ac_port_region_init(
        &regions[AC_PORT_REGION_STACK], 
        context->stacks[prio].top - context->stacks[prio].size,
        context->stacks[prio].size, 
        AC_ATTR_RW
    );
    ac_port_region_init(&regions[AC_REGION_MSG], 0, 0, AC_ATTR_RW);
    _mg_actor_activate(&actor->base);
}

static inline void ac_actor_allow(
    struct ac_actor_t* actor,
    size_t size,
    void* base,
    unsigned int attr
) {
    assert((size & (size - 1)) == 0);
    ac_port_region_init(
        &actor->granted[AC_REGION_USER], 
        (uintptr_t)base, 
        size, 
        attr
    );
}

static inline struct ac_port_frame_t* _ac_frame_create(
    struct ac_actor_t* actor
) {
    const struct ac_cpu_context_t* const context = AC_GET_CONTEXT();
    const unsigned int prio = actor->base.prio;
    const uintptr_t stack_top = context->stacks[prio].top;
    const bool restart_req = actor->restart_req;
    actor->restart_req = false;
    assert(stack_top != 0);
    return ac_port_frame_alloc(stack_top, actor->func, restart_req);
}

static inline struct ac_port_frame_t* _ac_intr_handler(
    uint32_t vect, 
    struct ac_port_frame_t* prev_frame
) {
    struct ac_cpu_context_t* const context = AC_GET_CONTEXT();
    struct ac_port_frame_t* frame = prev_frame;

    for (;;) {
        bool last = false;
        struct mg_actor_t* const next = _mg_context_pop_head(vect, &last);
        
        if (!next) {
            break; /* Spurious interrupt. No active actor at this level. */
        }

        if (next->func) {
            mg_actor_call(next);
        } else {
            struct ac_actor_t* const actor = (struct ac_actor_t*) next;
            const unsigned int prio = actor->base.prio;

            context->preempted[prio].frame = frame;
            context->preempted[prio].actor = context->running_actor;
            context->running_actor = actor;
            frame = _ac_frame_create(actor);
            ac_port_level_mask(prio);
            _ac_message_bind(actor);
            ac_port_mpu_reprogram(AC_REGIONS_NUM, actor->granted);
            ac_port_frame_set_arg(frame, actor->base.mailbox);

            if (!last) {
                pic_interrupt_request(mg_cpu_this(), vect);
            }
            break;
        }
    }

    return frame;
}

static inline struct ac_port_frame_t* _ac_frame_restore_prev(void) {
    struct ac_cpu_context_t* const context = AC_GET_CONTEXT();
    struct ac_actor_t* const me = context->running_actor;
    const unsigned int my_prio = me->base.prio;
    struct ac_actor_t* const prev = context->preempted[my_prio].actor;
    struct ac_port_frame_t* prev_frame = context->preempted[my_prio].frame;

    assert(me != 0);
    context->preempted[my_prio].frame = 0;
    context->preempted[my_prio].actor = 0;
    context->running_actor = prev;

    if (prev == 0) {       /* prev = 0 when returning into idle-loop */
        ac_port_level_mask(0);
        ac_port_mpu_reprogram(AC_REGIONS_NUM, context->granted);
    } else {
        ac_port_level_mask(prev->base.prio);
        ac_port_mpu_reprogram(AC_REGIONS_NUM, prev->granted);
    }

    return prev_frame;
}

static inline void ac_actor_restart(struct ac_actor_t* actor) {
    actor->restart_req = true;
    _mg_actor_activate(&actor->base);
}

static inline struct ac_port_frame_t* ac_actor_exception(void) {
    struct ac_cpu_context_t* const context = AC_GET_CONTEXT();
    struct ac_actor_t* const me = context->running_actor;
    assert(me != 0);
    _ac_message_release(me, true);
    ac_actor_error(me);
    return _ac_frame_restore_prev();
}

static inline bool _ac_sys_timeout(struct ac_actor_t* actor, uintptr_t req) {
    actor->base.timeout = (uint32_t) req;

    if (req) {
        _mg_actor_timeout(&actor->base);
    }

    return (req != 0);
}

static inline bool _ac_sys_subscribe(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_channel_t* const chan = ac_channel_validate(actor, req, false);
    bool is_async = true;

    if (chan) {
        _ac_message_release(actor, false);
        struct ac_message_t* msg = mg_message_alloc(&chan->base);
        
        if (msg == 0) {
            msg = (void*) mg_queue_pop(&chan->base.queue, &actor->base);
        }

        if (msg) {
            actor->base.mailbox = &msg->header;
            _ac_message_bind(actor);
            ac_port_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
            is_async = false;
        }
    }

    return is_async;
}

static inline void _ac_sys_push(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_channel_t* const chan = ac_channel_validate(actor, req, true);

    if (chan) {
        _ac_channel_push(actor, chan);
        ac_port_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
    }
}

static inline void _ac_sys_trypop(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_channel_t* const chan = ac_channel_validate(actor, req, false);

    if (chan) {
        _ac_message_release(actor, false);
        actor->base.mailbox = mg_message_alloc(&chan->base);
        _ac_message_bind(actor);
        ac_port_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
    }
}

static inline void _ac_sys_free(struct ac_actor_t* actor) {
    _ac_message_release(actor, false);
    ac_port_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
}

static inline struct ac_port_frame_t* _ac_svc_handler(
    uint32_t syscall, 
    struct ac_port_frame_t* prev_frame
) {
    const struct ac_cpu_context_t* const context = AC_GET_CONTEXT();    
    struct ac_actor_t* const actor = context->running_actor;
    struct ac_port_frame_t* frame = prev_frame;
    const uint32_t opcode = syscall >> 28;
    const uint32_t arg = syscall & UINT32_C(0x0fffffff);
    bool is_async = false;

    if (opcode < AC_CALL_MAX) {
        switch (opcode) {
        case AC_CALL_DELAY: 
            is_async = _ac_sys_timeout(actor, arg);
            break;
        case AC_CALL_SUBSCRIBE: 
            is_async = _ac_sys_subscribe(actor, arg); 
            break;
        case AC_CALL_TRY_POP:
            _ac_sys_trypop(actor, arg);
            break;
        case AC_CALL_PUSH: 
            _ac_sys_push(actor, arg);
            break;
        case AC_CALL_FREE:
            _ac_sys_free(actor);
            break;
        }

        if (is_async) {
            frame = _ac_frame_restore_prev();
        } else {
            ac_port_frame_set_arg(frame, actor->base.mailbox);
        }
    } else {
        frame = ac_actor_exception();
    }

    return frame;
}

#endif /* header guard */

