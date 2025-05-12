/*
 *  @file   ac_port.h
 *  @brief  ARMv8M hardware abstraction layer for the Actinium framework.
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
    frame->r0 = 0;
    frame->r1 = 0;
    frame->r2 = 0;
    frame->r3 = 0;
    frame->r12 = 0;
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
    asm volatile ("msr basepri, %0" : : "r" (value) );
}

struct ac_port_region_t {
    uint32_t rbar;
    uint32_t rlar;
};

//  MAIR values memo: 
//  - device memory nGnRE = 0x04, 
//  - normal memory outer shareable, WT/WA cacheable, nontransient = 0xaa.
//
//  Peripheral regions use MAIR index 0, other regions use MAIR index 1.
//  Attribute consists of two part: low byte holds RLAR status bits,
//  high byte holds RBAR status bits.
//

enum {
    AC_PORT_MAIR_VAL = 0xaa04,

    //  name     | SH + AP  |  XN bit  | mair idx | en
    AC_ATTR_RO = (0xb << 9) | (0 << 8) | (0 << 1) | 1,
    AC_ATTR_RW = (0x9 << 9) | (1 << 8) | (0 << 1) | 1,
    AC_ATTR_DEV= (0x9 << 9) | (1 << 8) | (1 << 1) | 1,
};

static inline void ac_port_region_init(
    struct ac_port_region_t* region, 
    uintptr_t addr, 
    size_t size,
    unsigned int attr
) {
    if (size != 0) {
        region->rbar = addr | (attr >> 8);
        region->rlar = ((addr + size - 1) & ~0x1fu) | (attr & 0xff);
    } else {
        region->rbar = 0;
        region->rlar = 0;
    }
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
        uint32_t rlar;
    } * const mpu = (void*) 0xe000ed90u;

    mpu->rnr = region_id;
    asm volatile ("dmb");
    mpu->rlar = 0;
    mpu->rbar = region->rbar;
    mpu->rlar = region->rlar;
    asm volatile ("dmb");
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
// This port supports SMP so this function should be called on each CPU.
// Since each CPU uses its own stack and CPUs number is unknown idle
// function's stack is allocated using the current stack pointer.
// Later kernel start procedure will read the stack pointer from the MPU
// directly.
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
    volatile uint32_t* const mair = (void*) 0xe000edc0u;
    volatile uint32_t* const mpu_ctrl = (void*) 0xe000ed94u;
    const size_t full_context_sz = 64;
    uintptr_t stack;

    asm volatile ("mov %0, sp" : "=r" (stack));
    stack &= ~31u; /* align down to 32 */
    stack -= full_context_sz;

    mair[0] = AC_PORT_MAIR_VAL;
    ac_port_region_init(&regions[AC_PORT_REGION_FLASH], ro_base, 64, AC_ATTR_RO);
    ac_port_region_init(&regions[AC_PORT_REGION_STACK], stack, full_context_sz, AC_ATTR_RW);
    ac_port_mpu_reprogram(sz, regions);
    ac_port_level_mask(2); /* blocks any user actor */

    *mpu_ctrl = MPU_PRIVDEFENA | MPU_ENABLE;
    asm volatile ("dsb");
    asm volatile ("isb");
}

#endif

