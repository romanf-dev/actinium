/** 
  ******************************************************************************
  *  @file   ac_port.h
  *  @brief  RISCV32 hardware abstraction layer for the Actinium framework.
  *****************************************************************************/

#ifndef AC_PORT_H
#define AC_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

enum {
    HAL_CONTEXT_SZ = 256, /* must be power of 2 */
    MPIE_BIT = 1 << 7,
    
    REG_RA  = 0,
    REG_SP  = 16 + 0,
    REG_GP  = 16 + 1,
    REG_TP  = 16 + 2,
    REG_T0  = 1,
    REG_T1  = 2,
    REG_T2  = 3,
    REG_S0  = 16 + 3,
    REG_S1  = 16 + 4,
    REG_A0  = 4,
    REG_A1  = 5,
    REG_A2  = 6,
    REG_A3  = 7,
    REG_A4  = 8,
    REG_A5  = 9,
    REG_A6  = 10,
    REG_A7  = 11,
    REG_S2  = 16 + 5,
    REG_S3  = 16 + 6,
    REG_S4  = 16 + 7,
    REG_S5  = 16 + 8,
    REG_S6  = 16 + 9,
    REG_S7  = 16 + 10,
    REG_S8  = 16 + 11,
    REG_S9  = 16 + 12,
    REG_S10 = 16 + 13,
    REG_S11 = 16 + 14,
    REG_T3  = 12,
    REG_T4  = 13,
    REG_T5  = 14,
    REG_T6  = 15,
};

/*
 * There are two distinct interrupt frames: small and full ones. Small frame
 * does not contain callee-saved registers, so register array contains only
 * 16 registers instead of 31. Registers are saved in the following order:
 * x1,x5-x7,x10-x11,x12-x17,x28-x31 then rest of the frame x2-x4,x8-x9,x18-x27.
 * Saved mstatus must have MIE=0 and MPIE=1 to avoid interrupts during 
 * context switch.
 */
struct hal_frame_t {
    uint32_t mstatus;
    uint32_t pc;
    uint32_t r[31];
};

static inline struct hal_frame_t* hal_frame_alloc(
    uintptr_t base,
    uintptr_t func,
    bool restart_marker
) {
    struct hal_frame_t* const frame = ((struct hal_frame_t*) base) - 1;
    frame->pc = func;
    frame->mstatus = MPIE_BIT;
    frame->r[REG_RA] = (uint32_t) restart_marker;
    frame->r[REG_SP] = base;
    return frame;
}

static inline void hal_frame_set_arg(struct hal_frame_t* frame, void* arg) {
    frame->r[REG_A0] = (uintptr_t)arg;
}

static inline void hal_intr_level(unsigned int level) {
    /* unused */
}

enum {
    AC_ATTR_RO = 0,
    AC_ATTR_RW = 1,
    AC_ATTR_DEV= 2,
};

struct hal_region_t {
    uint32_t addr;
};

static inline void hal_region_init(
    struct hal_region_t* region, 
    uintptr_t addr, 
    size_t size,
    unsigned int attr
) {
    /* TODO */
}

extern void hal_pmp_update_entry3(uint32_t);

static inline void hal_mpu_update_region(
    unsigned int i, 
    const struct hal_region_t* region
) {
    /* TODO */
}

static inline void hal_mpu_reprogram(
    size_t sz, 
    const struct hal_region_t* regions
) {
    /* TODO */
}

#endif

