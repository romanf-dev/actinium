/*
 *  @file   actinium.h
 *  @brief  Actinium interface for usermode actors.
 */

#ifndef ACTINIUM_TASK_H
#define ACTINIUM_TASK_H

#include <stddef.h>
#include <stdint.h>

enum {
    AC_SYSCALL_DELAY,
    AC_SYSCALL_SUBSCRIBE,
    AC_SYSCALL_TRY_POP,
    AC_SYSCALL_PUSH,
    AC_SYSCALL_FREE,
};

/* Tests may include both headers for kernel and user parts.
 * In that case usermode message definition is removed to use kernel
 * message definition which also contains all user-visible members.
 */
#ifndef ACTINIUM_H 

struct ac_message_t {
    size_t size;
    uintptr_t padding;
    uintptr_t poisoned;
};

#endif

extern void* _ac_syscall(uint32_t arg);

static inline uint32_t _ac_syscall_val(uint32_t id, uint32_t arg) {
    return (id << 28) | (arg & UINT32_C(0x0fffffff));
}

static inline uint32_t ac_sleep_for(uint32_t delay) {
    return _ac_syscall_val(AC_SYSCALL_DELAY, delay);
}

static inline uint32_t ac_subscribe_to(unsigned int id) {
    return _ac_syscall_val(AC_SYSCALL_SUBSCRIBE, id);
}

static inline void* ac_try_pop(unsigned int id) {
    return _ac_syscall(_ac_syscall_val(AC_SYSCALL_TRY_POP, id));
}

static inline void* ac_push(unsigned int id) {
    return _ac_syscall(_ac_syscall_val(AC_SYSCALL_PUSH, id));
}

static inline void ac_free(void) {
    (void) _ac_syscall(AC_SYSCALL_FREE << 28);
}

#define AC_ACTOR_START static int _ac_state = 0; switch(_ac_state) { case 0:
#define AC_ACTOR_END } return 0
#define AC_AWAIT(q) _ac_state = __LINE__; return (q); case __LINE__:

#endif

