/** 
  ******************************************************************************
  *  @file   ac_port.h
  *  @brief  ARMv7M hardware abstraction layer for the Actinium framework.
  *****************************************************************************/

#ifndef AC_PORT_H
#define AC_PORT_H

#include <stdbool.h>
#include "core_cm4.h"
#include "core_cmFunc.h"

struct hal_frame_t {
    uintptr_t r0;
    uintptr_t r1;
    uintptr_t r2;
    uintptr_t r3;
    uintptr_t r12;
    uintptr_t lr;
    uintptr_t pc;
    uintptr_t xPSR;
};

static inline uintptr_t hal_frame_alloc(
    uintptr_t base, 
    uintptr_t func, 
    uintptr_t arg, 
    bool restart_marker
) {
    struct hal_frame_t* const frame = ((struct hal_frame_t*) base) - 1;
    frame->r0 = arg;
    frame->r1 = 0;
    frame->r2 = 0;
    frame->r3 = 0;
    frame->r12 = 0;
    frame->lr = (restart_marker == true ) ? 1 : 0;
    frame->pc = func | 1;
    frame->xPSR = 1 << 24;
    return (uintptr_t)frame;
}

static inline void hal_frame_set_arg(uintptr_t base, uintptr_t arg) {
    struct hal_frame_t* const frame = (struct hal_frame_t*) base;
    frame->r0 = arg;
}

static inline size_t _hal_order(size_t n) {
    const uint32_t rev_bits = __RBIT(n);
    return __CLZ(rev_bits);
}

static inline void hal_intr_level(unsigned int level) {
    __set_BASEPRI(level << (8 - MG_NVIC_PRIO_BITS));
}

enum {
    AC_ATTR_RO = (6 << 24) | 1 << 17u | 1,
    AC_ATTR_RW = (3 << 24) | 3 << 17u | 1,
};

struct hal_mpu_region_t {
    uint32_t addr;
    uint32_t attr;
};

static inline void hal_mpu_region_init(
    struct hal_mpu_region_t* region, 
    uintptr_t addr, 
    size_t size,
    bool writable
) {
    const uint32_t size_mask = (_hal_order(size) - 1) << 1;
    const uint32_t attr_mask = writable ? AC_ATTR_RW : AC_ATTR_RO;
    region->addr = addr;
    region->attr = size ? (size_mask | attr_mask) : 0;
}

static inline void hal_mpu_update_region(
    unsigned int i, 
    const struct hal_mpu_region_t* region
) {
    MPU->RBAR = region->addr | (1 << 4) | i;
    MPU->RASR = 0;
    MPU->RASR = region->attr;
}

static inline void hal_mpu_reset(void) {
    MPU->RBAR = (1 << 4) | 0;
    MPU->RASR = 0;
    MPU->RASR = AC_ATTR_RW | (31 << 1);
}

static inline void hal_mpu_reprogram(
    size_t sz, 
    const struct hal_mpu_region_t* regions
) {
    __disable_irq();

    for (size_t i = 0; i < sz; ++i) {
        hal_mpu_update_region(i, &regions[i]);
    }

    __enable_irq();
}

#endif

