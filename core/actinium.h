/** 
  ******************************************************************************
  *  @file   actinium.h
  *  @brief  Implementation of the Actinium API. Functions with underline prefix
  *          are internal.
  *****************************************************************************/

#ifndef ACTINIUM_H
#define ACTINIUM_H

#include <stdint.h>
#include <stddef.h>

enum {
    AC_CALL_DELAY,
    AC_CALL_SUBSCRIBE,
    AC_CALL_LAST_ASYNC = AC_CALL_SUBSCRIBE, /* Only sync syscalls below. */
    AC_CALL_TRY_POP,
    AC_CALL_PUSH,
    AC_CALL_FREE,
    AC_CALL_MAX
};

#ifndef AC_TASK /* Part of the framework to be used by privileged code. */

#include <stdbool.h>
#include <stdnoreturn.h>
#include "magnesium.h"
#include "ac_port.h"

enum {
    AC_REGION_FLASH,
    AC_REGION_SRAM,
    AC_REGION_STACK,
    AC_REGION_MSG,
    AC_REGION_USER,
    AC_REGIONS_NUM,
    AC_RSVD_PRIO_NUM = 1 /* Priority 0 is reserved for exceptions. */
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
    struct hal_region_t granted[AC_REGIONS_NUM];
    uintptr_t func;
    bool restart_req;

    struct {
        struct ac_channel_t* parent;
        size_t size;
        int type;
    } msg;
};

struct ac_context_t {
    struct ac_actor_t* running_actor;
    struct hal_region_t granted[AC_REGIONS_NUM];

    struct {
        struct hal_frame_t* frame;
        struct ac_actor_t* actor;
    } preempted[MG_PRIO_MAX - AC_RSVD_PRIO_NUM];

    struct {
        uintptr_t addr;
        size_t size;
    } stacks[MG_PRIO_MAX - AC_RSVD_PRIO_NUM];
};

struct ac_channel_t {
    struct mg_message_pool_t base;
    int msg_type; /* Unique id of the message type. */
};

_Static_assert(offsetof(struct ac_message_t, header) == 0, "non 1st member");
_Static_assert(offsetof(struct ac_actor_t, base) == 0, "non 1st member");
_Static_assert(offsetof(struct ac_channel_t, base) == 0, "non 1st member");
_Static_assert(AC_REGION_STACK == 2, "asm part depends on stack region index");
_Static_assert(sizeof(struct ac_message_t) == sizeof(uintptr_t) * 4, "pad");

extern void noreturn ac_kernel_start(void);
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
#define AC_GET_CONTEXT() (&g_ac_context)

static inline void ac_context_init(uintptr_t ro_base, size_t ro_size) {
    struct ac_context_t* const context = AC_GET_CONTEXT();
    struct hal_region_t* const regions = &context->granted[0];
    extern const uint8_t _estack;
    const uintptr_t stack = (uintptr_t)&_estack - HAL_CONTEXT_SZ;

    mg_context_init();
    hal_region_init(&regions[AC_REGION_FLASH], ro_base, ro_size, AC_ATTR_RO);
    hal_region_init(&regions[AC_REGION_STACK], stack, HAL_CONTEXT_SZ, AC_ATTR_RW);
    hal_mpu_reprogram(AC_REGIONS_NUM, regions);
    hal_intr_level(AC_RSVD_PRIO_NUM); /* Max level for unprivileged actors. */
}

static inline void ac_context_tick(void) {
    mg_context_tick();
}

static inline void ac_context_stack_set(unsigned prio, size_t sz, void* ptr) {
    struct ac_context_t* const context = AC_GET_CONTEXT();
    const unsigned int level = prio - AC_RSVD_PRIO_NUM;

    assert((sz & (sz - 1)) == 0);
    assert(prio >= AC_RSVD_PRIO_NUM);
    assert(sz > HAL_CONTEXT_SZ);
    context->stacks[level].addr = (uintptr_t) ptr;
    context->stacks[level].size = sz;
}

static inline void ac_channel_init_ex(
    struct ac_channel_t* chan, 
    size_t total_len,
    void* mem,
    size_t block_sz,
    int msg_type
) {
    assert((block_sz & (block_sz - 1)) == 0);
    assert((((uintptr_t)mem) & (block_sz - 1)) == 0);
    assert(msg_type != 0);
    mg_queue_init(&chan->base.queue);
    chan->base.array = mem;
    chan->base.total_length = total_len;
    chan->base.block_sz = block_sz;
    chan->base.offset = 0;
    chan->base.array_space_available = (total_len != 0);
    chan->msg_type = msg_type;
}

static inline void ac_channel_init(struct ac_channel_t* chan, int msg_type) {
    ac_channel_init_ex(chan, 0, 0, 0, msg_type);
}

static inline void _ac_message_bind(struct ac_actor_t* actor) {
    struct ac_message_t* const msg = (void*) actor->base.mailbox;
    const bool not_bound = actor->msg.parent == 0;

    if (msg && not_bound) {
        struct ac_channel_t* const parent = (void*) msg->header.parent;
        struct hal_region_t* const region = &actor->granted[AC_REGION_MSG];       
        actor->msg.parent = parent;
        actor->msg.type = parent->msg_type;
        actor->msg.size = parent->base.block_sz;
        msg->size = actor->msg.size;
        hal_region_init(region, (uintptr_t)msg, actor->msg.size, AC_ATTR_RW);
    }
}

static inline void _ac_message_unbind(struct ac_actor_t* actor) {
    struct ac_message_t* const msg = (void*) actor->base.mailbox;
    assert(msg != 0);
    msg->header.parent = &actor->msg.parent->base;
    actor->base.mailbox = 0;
    actor->msg.parent = 0;
    actor->msg.type = 0;
    actor->msg.size = 0;
    hal_region_init(&actor->granted[AC_REGION_MSG], 0, 0, AC_ATTR_RW);
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

    if (msg && (src->msg.type == dst->msg_type)) {
        msg->poisoned = 0;
        _ac_message_unbind(src);
        mg_queue_push(&dst->base.queue, &msg->header);
    }
}

static inline void ac_actor_init(
    struct ac_actor_t* actor, 
    unsigned int vect,
    unsigned int task_id
) {
    extern const uintptr_t _ac_task_mem_map[]; /* From the linker script. */
    const uintptr_t* const config = (const uintptr_t*) &_ac_task_mem_map;
    const size_t task_num = config[0];
    struct hal_region_t* regions = actor->granted;
    const struct {
        uintptr_t flash_addr;
        uintptr_t flash_size;
        uintptr_t sram_addr; 
        uintptr_t sram_size;
    } * const slot = (void*) (config + 1);

    _Static_assert(sizeof(*slot) == sizeof(uintptr_t) * 4, "padding");
    assert(task_id < task_num);

    mg_actor_init(&actor->base, 0, vect, 0); /* Null func means usermode. */
    assert(actor->base.prio >= AC_RSVD_PRIO_NUM);
    actor->func = slot[task_id].flash_addr;
    actor->restart_req = true;
    actor->msg.parent = 0;
    actor->msg.size = 0;
    actor->msg.type = 0;

    const unsigned int level = actor->base.prio - AC_RSVD_PRIO_NUM;
    struct ac_context_t* const context = AC_GET_CONTEXT();

    hal_region_init(
        &regions[AC_REGION_FLASH], 
        slot[task_id].flash_addr, 
        slot[task_id].flash_size, 
        AC_ATTR_RO
    );
    hal_region_init(
        &regions[AC_REGION_SRAM], 
        slot[task_id].sram_addr, 
        slot[task_id].sram_size, 
        AC_ATTR_RW
    );
    hal_region_init(
        &regions[AC_REGION_STACK], 
        context->stacks[level].addr, 
        context->stacks[level].size, 
        AC_ATTR_RW
    );
    hal_region_init(&regions[AC_REGION_MSG], 0, 0, AC_ATTR_RW);
    _mg_actor_activate(&actor->base);
}

static inline void ac_actor_allow(
    struct ac_actor_t* actor,
    size_t size,
    void* base,
    unsigned int attr
) {
    assert((size & (size - 1)) == 0);
    hal_region_init(&actor->granted[AC_REGION_USER], (uintptr_t)base, size, attr);
}

static inline struct hal_frame_t* _ac_frame_create(struct ac_actor_t* actor) {
    const struct ac_context_t* const context = AC_GET_CONTEXT();
    const unsigned int prio = actor->base.prio - AC_RSVD_PRIO_NUM;
    const uintptr_t base = context->stacks[prio].addr;
    const size_t size = context->stacks[prio].size;
    const uintptr_t stack_top = base + size;
    const bool restart_req = actor->restart_req;

    actor->restart_req = false;
    assert(base != 0);
    return hal_frame_alloc(stack_top, actor->func, restart_req);
}

static inline struct hal_frame_t* _ac_intr_handler(
    uint32_t vect, 
    struct hal_frame_t* prev_frame
) {
    struct ac_context_t* const context = AC_GET_CONTEXT();
    struct hal_frame_t* frame = prev_frame;

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
            const unsigned int level = actor->base.prio - AC_RSVD_PRIO_NUM;

            context->preempted[level].frame = frame;
            context->preempted[level].actor = context->running_actor;
            context->running_actor = actor;
            frame = _ac_frame_create(actor);
            hal_intr_level(actor->base.prio);
            _ac_message_bind(actor);
            hal_mpu_reprogram(AC_REGIONS_NUM, actor->granted);
            hal_frame_set_arg(frame, actor->base.mailbox);

            if (!last) {
                pic_interrupt_request(vect);
            }
            break;
        }
    }

    return frame;
}

static inline struct hal_frame_t* _ac_frame_restore_prev(void) {
    struct ac_context_t* const context = AC_GET_CONTEXT();
    struct ac_actor_t* const me = context->running_actor;
    const unsigned int my_prio = me->base.prio - AC_RSVD_PRIO_NUM;
    struct ac_actor_t* const prev = context->preempted[my_prio].actor;
    struct hal_frame_t* prev_frame = context->preempted[my_prio].frame;

    assert(me != 0);
    context->preempted[my_prio].frame = 0;
    context->preempted[my_prio].actor = 0;
    context->running_actor = prev;

    if (prev == 0) {       /* prev = 0 when returning into idle-loop */
        hal_intr_level(0);
        hal_mpu_reprogram(AC_REGIONS_NUM, context->granted);
    } else {
        hal_intr_level(prev->base.prio);
        hal_mpu_reprogram(AC_REGIONS_NUM, prev->granted);
    }

    return prev_frame;
}

static inline void ac_actor_restart(struct ac_actor_t* actor) {
    actor->restart_req = true;
    _mg_actor_activate(&actor->base);
}

static inline struct hal_frame_t* ac_actor_exception(void) {
    struct ac_context_t* const context = AC_GET_CONTEXT();
    struct ac_actor_t* const me = context->running_actor;
    assert(me != 0);
    _ac_message_release(me, true);
    ac_actor_error(me);

    return _ac_frame_restore_prev();
}

static inline void _ac_sys_timeout(struct ac_actor_t* actor, uintptr_t req) {
    actor->base.timeout = (uint32_t) req;

    if (req) {
        _mg_actor_timeout(actor->base.parent, &actor->base);
    } else {
        _mg_actor_activate(&actor->base);
    }
}

static inline void _ac_sys_subscribe(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_channel_t* const chan = ac_channel_validate(actor, req, false);

    if (chan) {
        _ac_message_release(actor, false);
        struct ac_message_t* msg = mg_message_alloc(&chan->base);
        
        if (msg == 0) {
            msg = (void*) mg_queue_pop(&chan->base.queue, &actor->base);
        }

        if (msg) {
            actor->base.mailbox = &msg->header;
            _mg_actor_activate(&actor->base);
        }
    }
}

static inline void _ac_sys_push(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_channel_t* const chan = ac_channel_validate(actor, req, true);

    if (chan) {
        _ac_channel_push(actor, chan);
        hal_mpu_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
    }
}

static inline void _ac_sys_trypop(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_channel_t* const chan = ac_channel_validate(actor, req, false);

    if (chan) {
        _ac_message_release(actor, false);
        actor->base.mailbox = mg_message_alloc(&chan->base);
        _ac_message_bind(actor);
        hal_mpu_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
    }
}

static inline void _ac_sys_free(struct ac_actor_t* actor) {
    _ac_message_release(actor, false);
    hal_mpu_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
}

static inline struct hal_frame_t* _ac_svc_handler(
    uint32_t r0, 
    struct hal_frame_t* prev_frame
) {
    const struct ac_context_t* const context = AC_GET_CONTEXT();    
    struct ac_actor_t* const actor = context->running_actor;
    struct hal_frame_t* frame = prev_frame;
    const uint32_t opcode = r0 >> 28;
    const uint32_t arg = r0 & UINT32_C(0x0fffffff);

    if (opcode < AC_CALL_MAX) {
        switch (opcode) {
        case AC_CALL_DELAY: 
            _ac_sys_timeout(actor, arg);
            break;
        case AC_CALL_SUBSCRIBE: 
            _ac_sys_subscribe(actor, arg); 
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

        if (opcode > AC_CALL_LAST_ASYNC) {
            hal_frame_set_arg(frame, actor->base.mailbox);
        } else {
            frame = _ac_frame_restore_prev();
        }
    } else {
        frame = ac_actor_exception();
    }

    return frame;
}

#else /* ifndef AC_TASK */

struct ac_message_t {
    size_t size;
    uintptr_t padding[2];
    uintptr_t poisoned;
};

extern void* _ac_syscall(uint32_t arg);

static inline uint32_t _ac_syscall_val(uint32_t id, uint32_t arg) {
    return (id << 28) | (arg & UINT32_C(0x0fffffff));
}

static inline uint32_t act_sleep_for(uint32_t delay) {
    return _ac_syscall_val(AC_CALL_DELAY, delay);
}

static inline uint32_t act_subscribe_to(unsigned int id) {
    return _ac_syscall_val(AC_CALL_SUBSCRIBE, id);
}

static inline void* act_try_pop(unsigned int id) {
    return _ac_syscall(_ac_syscall_val(AC_CALL_TRY_POP, id));
}

static inline void* act_push(unsigned int id) {
    return _ac_syscall(_ac_syscall_val(AC_CALL_PUSH, id));
}

static inline void act_free(void) {
    (void) _ac_syscall(AC_CALL_FREE << 28);
}

#define AC_ACTOR_START static int _ac_state = 0; switch(_ac_state) { case 0:
#define AC_ACTOR_END } return 0
#define AC_AWAIT(q) _ac_state = __LINE__; return (q); case __LINE__:

#endif /* ifndef AC_TASK */
#endif /* header guard */

