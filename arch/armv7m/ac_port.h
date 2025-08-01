/*
 *  @file   ac_port.h
 *  @brief  ARMv7M hardware abstraction layer for the Actinium framework.
 *
 *  Design notes:
 *  - LR register is used as a 'full restart marker'. Nonzero LR on actor 
 *    activation means that this is either first call or the call following
 *    actor's crash so full initialization of data/bss sections should be 
 *    performed.
 *  - Priority levels 0 and 1 are reserved for exceptions and the tick 
 *    respectively. These priorities must not be used for actors.
 *  - Exception return procedure acts as a barrier for MPU reprogramming so
 *    no need for explicit DSB/ISB.
 */

#ifndef AC_PORT_H
#define AC_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mg_port.h"

#if !defined (__GNUC__)
#error This file may be used in GNU GCC only because of non-portable functions.
#endif

enum {
    AC_PORT_REGION_FLASH,
    AC_PORT_REGION_SRAM,
    AC_PORT_REGION_STACK,
    AC_PORT_REGIONS_NUM
};

struct ac_port_frame_t {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xPSR;
};

static inline struct ac_port_frame_t* ac_port_frame_alloc(
    uintptr_t base, 
    uintptr_t func, 
    bool restart_marker
) {
    struct ac_port_frame_t* const frame = ((struct ac_port_frame_t*) base) - 1;
    frame->lr = restart_marker;
    frame->pc = func | 1; /* thumb bit */
    frame->xPSR = 1 << 24;

    return frame;
}

static inline void ac_port_frame_set_arg(
    struct ac_port_frame_t* frame, 
    void* arg
) {
    frame->r0 = (uintptr_t)arg;
}

static inline void ac_port_level_mask(unsigned int level) {
    const uint32_t value = level << (8 - MG_NVIC_PRIO_BITS);
    asm volatile ("MSR basepri, %0" : : "r" (value) );
}

enum {
    //   name    |   XN    |  AP[2:0]  | S C B bits | ENABLED
    AC_ATTR_RO =             (6 << 24) | (2 << 16)  | 1,
    AC_ATTR_RW = (1 << 28) | (3 << 24) | (6 << 16)  | 1,
    AC_ATTR_DEV= (1 << 28) | (3 << 24) | (5 << 16)  | 1,
};

struct ac_port_region_t {
    uint32_t addr;
    uint32_t attr;
};

static inline void ac_port_region_init(
    struct ac_port_region_t* region, 
    uintptr_t addr, 
    size_t size,
    unsigned int attr
) {
    const size_t msb = 31 - mg_port_clz(size);
    const uint32_t size_mask = (msb - 1) << 1;
    region->addr = addr;
    region->attr = size ? (size_mask | attr) : 0;
}

static inline void ac_port_update_region_nosync(
    unsigned int region_id, 
    const struct ac_port_region_t* region
) {
    volatile struct {
        uint32_t type;
        uint32_t ctrl;
        uint32_t rnr; 
        uint32_t rbar;
        uint32_t rasr;
    } * const mpu = (void*) 0xE000ED90UL;

    mpu->rbar = region->addr | (1 << 4) | region_id;
    mpu->rasr = region->attr;
}

static inline void ac_port_update_region(
    unsigned int region_id, 
    const struct ac_port_region_t* region
) {
    mg_critical_section_enter();
    ac_port_update_region_nosync(region_id, region);
    mg_critical_section_leave();
}

static inline void ac_port_mpu_reprogram(
    size_t sz, 
    const struct ac_port_region_t* regions
) {
    mg_critical_section_enter();

    for (size_t i = 0; i < sz; ++i) {
        ac_port_update_region_nosync(i, &regions[i]);
    }

    mg_critical_section_leave();
}

//
// The kernel sets CONTROL just once so code of 'idle' is also subjected
// to MPU restrictions. This code sets up minimal MPU regions required for 
// idle execution. The asm part defines the special symbol pointing to the 
// code of the idle function. By convention the code is aligned to 64. Idle 
// stack is derived from the current SP and should be large enough to hold the
// full context (64 bytes). The asm part reads SP directly from the MPU 
// before the idle loop is started. It is expected that this code is called
// within the main() so SP is near to its initial value.
//

static inline void ac_port_init(
    size_t sz, 
    struct ac_port_region_t regions[static sz]
) {
    enum {
        MPU_PRIVDEFENA = 4,
        MPU_ENABLE = 1,
    };
    extern const uint8_t ac_port_idle_text;
    const uintptr_t ro_base = (uintptr_t)&ac_port_idle_text;
    const size_t full_context_sz = 64;
    volatile uint32_t* const mpu_ctrl = (void*) 0xe000ed94u;
    uintptr_t stack;

    asm volatile ("mov %0, sp" : "=r" (stack));
    stack &= ~(full_context_sz - 1);
    stack -= full_context_sz;

    ac_port_region_init(&regions[AC_PORT_REGION_FLASH], ro_base, 64, AC_ATTR_RO);
    ac_port_region_init(&regions[AC_PORT_REGION_STACK], stack, full_context_sz, AC_ATTR_RW);
    ac_port_mpu_reprogram(sz, regions);
    ac_port_level_mask(2); /* blocks any user actor */

    *mpu_ctrl = MPU_PRIVDEFENA | MPU_ENABLE;
    asm volatile ("dsb");
    asm volatile ("isb");
}

#endif

