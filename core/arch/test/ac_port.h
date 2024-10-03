/** 
  ******************************************************************************
  *  @file   ac_port.h
  *  @brief  
  *****************************************************************************/

#ifndef AC_PORT_H
#define AC_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <setjmp.h>

enum {
    HAL_CONTEXT_SZ = 1, /* must be power of 2 */
};

struct hal_frame_t {
    void* arg;
    uint32_t (*func)(void*);
    unsigned int restart;
    jmp_buf context;
};

static inline struct hal_frame_t* hal_frame_alloc(
    uintptr_t base,
    uintptr_t func,
    bool restart_marker
) {
    static struct hal_frame_t internal_frame;
    internal_frame.func = (uint32_t (*)(void*)) func;
    internal_frame.restart = restart_marker;
    return &internal_frame;
}

static inline void hal_frame_set_arg(struct hal_frame_t* frame, void* arg) {
    frame->arg = arg;
}

static inline void hal_intr_level(unsigned int level) {
    /*TODO*/
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
    /*TODO*/
}

static inline void hal_mpu_update_region(
    unsigned int i, 
    struct hal_region_t* region
) {
    /*TODO*/
}

static inline void hal_mpu_reprogram(
    size_t sz, 
    struct hal_region_t* regions
) {   
    /*TODO*/
}

#endif

