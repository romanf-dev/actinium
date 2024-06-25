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

enum {
    HAL_CONTEXT_SZ = 64 // full context size including high registers
};

struct hal_frame_t {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xPSR;
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
    frame->lr = restart_marker ? 1 : 0;
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
/*     name      |   XN    |  AP[2:0]  | S C B bits | ENABLED */
    AC_ATTR_RO =             (6 << 24) | (2 << 16)  | 1,
    AC_ATTR_RW = (1 << 28) | (3 << 24) | (6 << 16)  | 1,
    AC_ATTR_DEV= (1 << 28) | (3 << 24) | (5 << 16)  | 1,
};

struct hal_mpu_region_t {
    uint32_t addr;
    uint32_t attr;
};

static inline void hal_mpu_region_init(
    struct hal_mpu_region_t* region, 
    uintptr_t addr, 
    size_t size,
    unsigned int attr
) {
    const uint32_t size_mask = (_hal_order(size) - 1) << MPU_RASR_SIZE_Pos;
    region->addr = addr;
    region->attr = size ? (size_mask | attr) : 0;
}

static inline void hal_mpu_update_region_nosync(
    unsigned int i, 
    const struct hal_mpu_region_t* region
) {
    MPU->RBAR = region->addr | (1 << MPU_RBAR_VALID_Pos) | i;
    MPU->RASR = 0;
    MPU->RASR = region->attr;
}

static inline void hal_mpu_update_region(
    unsigned int i, 
    const struct hal_mpu_region_t* region
) {
    __disable_irq();
    hal_mpu_update_region_nosync(i, region);
    __enable_irq();
}

static inline void hal_mpu_reprogram(
    size_t sz, 
    const struct hal_mpu_region_t* regions
) {
    __disable_irq();

    for (size_t i = 0; i < sz; ++i) {
        hal_mpu_update_region_nosync(i, &regions[i]);
    }

    __enable_irq();
}

#endif

