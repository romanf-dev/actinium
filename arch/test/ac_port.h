/* 
 *  @file  ac_port.h
 *  @brief Porting layer for the hosted environment.
 */

#ifndef AC_PORT_H
#define AC_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <setjmp.h>

enum {
    AC_PORT_REGION_FLASH,
    AC_PORT_REGION_SRAM,
    AC_PORT_REGION_STACK,
    AC_PORT_REGIONS_NUM
};

struct ac_port_frame_t {
    void* arg;
    uint32_t (*func)(void*);
    unsigned int restart;
    jmp_buf context;
};

static inline struct ac_port_frame_t* ac_port_frame_alloc(
    uintptr_t base,
    uintptr_t func,
    bool restart_marker
) {
    static struct ac_port_frame_t internal_frame;
    internal_frame.func = (uint32_t (*)(void*)) func;
    internal_frame.restart = restart_marker;
    return &internal_frame;
}

static inline void ac_port_frame_set_arg(
    struct ac_port_frame_t* frame, 
    void* arg
) {
    frame->arg = arg;
}

static inline void ac_port_level_mask(unsigned int level) {

}

enum {
    AC_ATTR_RO,
    AC_ATTR_RW,
    AC_ATTR_DEV,
};

struct ac_port_region_t {
    uint32_t addr;
};

static inline void ac_port_region_init(
    struct ac_port_region_t* region, 
    uintptr_t addr, 
    size_t size,
    unsigned int attr
) {
    /*TODO*/
}

static inline void ac_port_update_region(
    unsigned int i, 
    struct ac_port_region_t* region
) {
    /*TODO*/
}

static inline void ac_port_mpu_reprogram(
    size_t sz, 
    struct ac_port_region_t* regions
) {   
    /*TODO*/
}

static inline void ac_port_init(
    size_t sz, 
    struct ac_port_region_t regions[static sz]
) {

}

void ac_port_swi_handler(void);

#endif

