/** 
  ******************************************************************************
  *  @file   actinium.h
  *  @brief  Implementation of the Actinium API. Functions with underline prefix
  *          are internal.
  *****************************************************************************/

#ifndef ACTINIUM_H
#define ACTINIUM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <stddef.h>
#include "magnesium.h"
#include "ac_port.h"

enum {
    AC_REGION_FLASH,
    AC_REGION_SRAM,
    AC_REGION_STACK,
    AC_REGION_MSG,
    AC_REGION_USER,
    AC_REGIONS_NUM
};

enum {
    AC_CALL_DELAY,
    AC_CALL_SUBSCRIBE,
    AC_CALL_ALLOC_ASYNC,
    _AC_CALL_LAST_ASYNC = AC_CALL_ALLOC_ASYNC, /* only sync syscalls below */
    AC_CALL_TRY_POP,
    AC_CALL_ALLOC_SYNC,
    AC_CALL_PUSH,
    AC_CALL_FREE,
    AC_CALL_MAX
};

struct ac_message_t {
    struct mg_message_t header;
    uintptr_t poisoned; /* only uintptrs in MG header, use it for alignment */
};

struct ac_actor_t {
    struct mg_actor_t base;
    struct hal_mpu_region_t granted[AC_REGIONS_NUM];
    uintptr_t func;
    bool restart_req;

    struct {
        struct ac_message_pool_t* parent;
        size_t size;
        int type;
    } msg;
};

struct ac_context_t {
    struct ac_actor_t* running_actor;
    struct hal_mpu_region_t granted[AC_REGIONS_NUM];

    struct {
        uintptr_t frame;
        struct ac_actor_t* actor;
    } preempted[MG_PRIO_MAX - 1];

    struct {
        uintptr_t addr;
        size_t size;
    } stacks[MG_PRIO_MAX - 1];
};

struct ac_message_pool_t {
    struct mg_message_pool_t base;
    int msg_type;
};

struct ac_channel_t {
    struct mg_queue_t base;
    int msg_type;
};

_Static_assert(offsetof(struct ac_message_t, header) == 0, "non 1st member");
_Static_assert(offsetof(struct ac_actor_t, base) == 0, "non 1st member");
_Static_assert(offsetof(struct ac_message_pool_t, base) == 0, "non 1st member");
_Static_assert(offsetof(struct ac_channel_t, base) == 0, "non 1st member");
_Static_assert(AC_REGION_STACK == 2, "asm part depends on stack region index");

extern struct ac_channel_t* ac_channel_validate(
    struct ac_actor_t* actor, 
    unsigned int handle
);

extern struct ac_message_pool_t* ac_pool_validate(
    struct ac_actor_t* actor, 
    unsigned int handle
);

extern void noreturn ac_kernel_start(void);
extern const uintptr_t _ac_task_mem_map[];
extern struct ac_context_t g_ac_context;
#define AC_GET_CONTEXT() (&g_ac_context)

static inline void ac_context_init(uintptr_t ro_base, size_t ro_size) {
    struct ac_context_t* context = AC_GET_CONTEXT();
    extern const uint8_t _estack;
    const uintptr_t idle_stack = (uintptr_t)&_estack - HAL_CONTEXT_SZ;
    struct hal_mpu_region_t* const idle_mem = context->granted;

    mg_context_init();
    hal_mpu_region_init(&idle_mem[AC_REGION_FLASH], ro_base, ro_size, AC_ATTR_RO);
    hal_mpu_region_init(&idle_mem[AC_REGION_STACK], idle_stack, HAL_CONTEXT_SZ, AC_ATTR_RW);
    hal_mpu_reprogram(AC_REGIONS_NUM, context->granted);
    hal_intr_level(1); /* max possible level for unprivileged actors */
}

static inline void ac_message_pool_init(
    struct ac_message_pool_t* pool, 
    void* mem, 
    size_t total_len, 
    size_t block_sz,
    int msg_type
) {
    assert((block_sz & (block_sz - 1)) == 0);
    assert((((uintptr_t)mem) & (block_sz - 1)) == 0);
    mg_message_pool_init(&pool->base, mem, total_len, block_sz);
    pool->msg_type = msg_type;
}

static inline void ac_channel_init(struct ac_channel_t* chan, int msg_type) {
    mg_queue_init(&chan->base);
    chan->msg_type = msg_type;
}

static inline void _ac_message_set(struct ac_actor_t* actor) {
    struct ac_message_t* const msg = (struct ac_message_t*) actor->base.mailbox;

    if (msg && (actor->msg.parent == 0)) {
        struct ac_message_pool_t* const parent = (void*) msg->header.parent;
        struct hal_mpu_region_t* region = &actor->granted[AC_REGION_MSG];       
        actor->msg.parent = parent;
        actor->msg.type = parent->msg_type;
        actor->msg.size = parent->base.block_sz;
        msg->header.parent = 0;
        hal_mpu_region_init(region, (uintptr_t)msg, actor->msg.size, AC_ATTR_RW);
    }
}

static inline void _ac_message_cleanup(struct ac_actor_t* actor) {
    struct ac_message_t* const msg = (struct ac_message_t*) actor->base.mailbox;
    assert(msg != 0);
    msg->header.parent = &actor->msg.parent->base;
    actor->base.mailbox = 0;
    actor->msg.parent = 0;
    actor->msg.type = 0;
    actor->msg.size = 0;
    hal_mpu_region_init(&actor->granted[AC_REGION_MSG], 0, 0, AC_ATTR_RW);
}

static inline void _ac_message_release(struct ac_actor_t* actor, bool poisoned) {
    struct ac_message_t* const msg = (struct ac_message_t*) actor->base.mailbox;

    if (msg) {
        _ac_message_cleanup(actor);
        msg->poisoned = (uintptr_t) poisoned;
        mg_message_free(&msg->header);
    }
}

static inline void _ac_channel_push(
    struct ac_actor_t* src, 
    struct ac_channel_t* dst
) {
    struct ac_message_t* const msg = (struct ac_message_t*) src->base.mailbox;

    if (msg) {
        if (src->msg.type == dst->msg_type) { /* typechecking */
            msg->poisoned = 0;
            _ac_message_cleanup(src);
            mg_queue_push(&dst->base, &msg->header);
        }
    }
}

static inline void ac_actor_init(
    struct ac_actor_t* actor, 
    unsigned int vect,
    unsigned int task_id
) {
    struct ac_context_t* context = AC_GET_CONTEXT();
    const uintptr_t* const config = (const uintptr_t*) &_ac_task_mem_map;
    struct hal_mpu_region_t* region = actor->granted;
    const struct {
        uint32_t flash_addr;
        uint32_t flash_size;
        uint32_t sram_addr; 
        uint32_t sram_size;
    } * const slot = (void*) (config + 1);

    _Static_assert(sizeof(*slot) == sizeof(uint32_t) * 4, "wrong slot type");

    mg_actor_init(&actor->base, 0, vect, 0); /* zero func means usermode. */
    actor->func = slot[task_id].flash_addr;
    actor->restart_req = true;
    actor->msg.parent = 0;
    actor->msg.size = 0;
    actor->msg.type = 0;

    const size_t prio = actor->base.prio - 1;

    hal_mpu_region_init(
        &region[AC_REGION_FLASH], 
        slot[task_id].flash_addr, 
        slot[task_id].flash_size, 
        AC_ATTR_RO
    );
    hal_mpu_region_init(
        &region[AC_REGION_SRAM], 
        slot[task_id].sram_addr, 
        slot[task_id].sram_size, 
        AC_ATTR_RW
    );
    hal_mpu_region_init(
        &region[AC_REGION_STACK], 
        context->stacks[prio].addr, 
        context->stacks[prio].size, 
        AC_ATTR_RW
    );

    hal_mpu_region_init(&region[AC_REGION_MSG], 0, 0, AC_ATTR_RW);
    _mg_actor_activate(&actor->base);
}

static inline uintptr_t _ac_actor_create_frame(struct ac_actor_t* actor) {
    struct ac_context_t* context = AC_GET_CONTEXT();
    const unsigned int prio = actor->base.prio - 1;
    const uintptr_t base = context->stacks[prio].addr;
    const uintptr_t size = context->stacks[prio].size;
    const uintptr_t stack_top = base + size;
    const bool restart_req = actor->restart_req;
    actor->restart_req = false;

    return hal_frame_alloc(stack_top, actor->func, 0, restart_req);
}

static inline uintptr_t _ac_intr_handler(uint32_t vect, uintptr_t old_frame) {
    struct ac_context_t* const context = AC_GET_CONTEXT();
    uintptr_t frame = old_frame;

    for (;;) {
        bool last = false;
        struct mg_actor_t* const next = _mg_context_pop_head(vect, &last);
        
        if (!next) {
            break; 
        }

        if (next->func) {
            mg_actor_call(next);
        } else {
            struct ac_actor_t* const actor = (struct ac_actor_t*) next;
            const unsigned int prio = actor->base.prio - 1;

            /* frame may be non-0 for the same actor in the case of exception */
            if (context->preempted[prio].frame == 0) { 
                context->preempted[prio].frame = frame;
                context->preempted[prio].actor = context->running_actor;
            }

            context->running_actor = actor;
            frame = _ac_actor_create_frame(actor);
            hal_intr_level(actor->base.prio);
            _ac_message_set(actor);
            hal_mpu_reprogram(AC_REGIONS_NUM, actor->granted);
            hal_frame_set_arg(frame, (uintptr_t)(actor->base.mailbox));

            if (!last) {
                pic_interrupt_request(vect);
            }
            break;
        }
    }

    return frame;
}

static inline uintptr_t _ac_actor_restore_prev() {
    struct ac_context_t* const context = AC_GET_CONTEXT();
    struct ac_actor_t* const me = context->running_actor;
    const unsigned int my_prio = me->base.prio - 1;
    struct ac_actor_t* const prev = context->preempted[my_prio].actor;
    uintptr_t prev_frame = context->preempted[my_prio].frame;

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

static inline uintptr_t ac_actor_exception(void) {
    struct ac_context_t* const context = AC_GET_CONTEXT();
    struct ac_actor_t* const me = context->running_actor;
    const unsigned int prio = me->base.prio - 1;
    me->restart_req = true;
    _ac_message_release(me, true);
    _mg_actor_activate(&me->base);

    if(context->preempted[prio].actor) {
        hal_intr_level(context->preempted[prio].actor->base.prio);
    } else {
        hal_intr_level(0);
    }

    return context->preempted[prio].frame;
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
    struct ac_channel_t* const chan = ac_channel_validate(actor, req);

    if (chan) {
        _ac_message_release(actor, false);
        struct mg_message_t* const msg = mg_queue_pop(&chan->base, &actor->base);

        if (msg) {
            actor->base.mailbox = msg;
            _mg_actor_activate(&actor->base);
        }
    }
}

static inline void _ac_sys_push(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_channel_t* const chan = ac_channel_validate(actor, req);

    if (chan) {
        _ac_channel_push(actor, chan);
        hal_mpu_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
    }
}

static inline void _ac_sys_trypop(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_channel_t* const chan = ac_channel_validate(actor, req);

    if (chan) {
        _ac_message_release(actor, false);
        actor->base.mailbox = mg_queue_pop(&chan->base, 0);
        _ac_message_set(actor);
        hal_mpu_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
    }
}

static inline void _ac_sys_alloc(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_message_pool_t* const pool = ac_pool_validate(actor, req);

    if (pool) {
        _ac_message_release(actor, false);
        actor->base.mailbox = mg_message_alloc(&pool->base);
        _ac_message_set(actor);
        hal_mpu_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
    }
}

static inline void _ac_sys_free(struct ac_actor_t* actor) {
    _ac_message_release(actor, false);
    hal_mpu_update_region(AC_REGION_MSG, &actor->granted[AC_REGION_MSG]);
}

static inline void _ac_sys_alloc_async(struct ac_actor_t* actor, uintptr_t req) {
    struct ac_message_pool_t* const pool = ac_pool_validate(actor, req);

    if (pool) {
        _ac_message_release(actor, false);
        struct ac_message_t* msg = mg_message_alloc(&pool->base);
        
        if (msg == 0) {
            msg = (void*) mg_queue_pop(&pool->base.queue, &actor->base);
        }

        if (msg) {
            actor->base.mailbox = &msg->header;
            _mg_actor_activate(&actor->base);
        }
    }
}

static inline uintptr_t _ac_svc_handler(uint32_t r0, uintptr_t prev_frame) {
    const struct ac_context_t* const context = AC_GET_CONTEXT();    
    struct ac_actor_t* const actor = context->running_actor;
    uintptr_t frame = prev_frame;
    const uint32_t opcode = r0 >> 28;
    const uint32_t arg = r0 & UINT32_C(0x0fffffff);
    bool sync = opcode > _AC_CALL_LAST_ASYNC;

    if (frame && (opcode < AC_CALL_MAX)) {
        switch (opcode) {
        case AC_CALL_DELAY: 
            _ac_sys_timeout(actor, arg);
            break;
        case AC_CALL_SUBSCRIBE: 
            _ac_sys_subscribe(actor, arg); 
            break;
        case AC_CALL_ALLOC_ASYNC:
            _ac_sys_alloc_async(actor, arg);
            break;
        case AC_CALL_TRY_POP:
            _ac_sys_trypop(actor, arg);
            break;
        case AC_CALL_ALLOC_SYNC:
            _ac_sys_alloc(actor, arg);
            break;
        case AC_CALL_PUSH: 
            _ac_sys_push(actor, arg);
            break;
        case AC_CALL_FREE:
            _ac_sys_free(actor);
            break;
        }
    } else {
        sync = false;
        ac_actor_exception();
    }

    if (sync) {
        hal_frame_set_arg(frame, (uintptr_t)(actor->base.mailbox));
    } else {
        frame = _ac_actor_restore_prev();
    }

    return frame;
}

extern void* _ac_syscall(uintptr_t arg);

static inline uint32_t syscall_val(uint32_t id, uint32_t arg) {
    return (id << 28) | (arg & UINT32_C(0x0fffffff));
}

static inline uint32_t act_sleep_for(uint32_t delay) {
    return syscall_val(AC_CALL_DELAY, delay);
}

static inline uint32_t act_subscribe_to(unsigned int id) {
    return syscall_val(AC_CALL_SUBSCRIBE, id);
}

static inline uint32_t act_async_alloc(unsigned int id) {
    return syscall_val(AC_CALL_ALLOC_ASYNC, id);
}

static inline void* act_try_pop(unsigned int id) {
    return _ac_syscall(syscall_val(AC_CALL_TRY_POP, id));
}

static inline void* act_alloc(unsigned int id) {
    return _ac_syscall(syscall_val(AC_CALL_ALLOC_SYNC, id));
}

static inline void* act_push(unsigned int id) {
    return _ac_syscall(syscall_val(AC_CALL_PUSH, id));
}

static inline void act_free(void) {
    (void) _ac_syscall(AC_CALL_FREE << 28);
}

#define AC_ACTOR_START static int _ac_state = 0; switch(_ac_state) { case 0:
#define AC_ACTOR_END } return 0
#define AC_AWAIT(q) _ac_state = __LINE__; return (q); case __LINE__:

#endif

