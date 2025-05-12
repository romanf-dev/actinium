/*
 *  @file   ac_gpic.h
 *  @brief  Generic Programmable Interrupt Controller (PIC) implementation.
 *          Simulates multiple irq 'vectors' inside a single one.
 *
 *  Design description:
 *  This is a pure software implementation compatible with any interrupt
 *  controller. The module maintains 32 'vectors' available for actor execution.
 *  These vectors are represented as three dwords: masked, pending, active.
 *  A single software IRQ is used as a workhorse for the preemption: 
 *  whenever there is unmasked vector the AC_GPIC_REQUEST is used to request 
 *  underlying software interrupt. Pending bits are reset upon entering
 *  the interrupt handler.
 */

#ifndef AC_GPIC_H
#define AC_GPIC_H

#include <stdint.h>

#ifndef AC_GPIC_CLZ
#define AC_GPIC_CLZ(v) __builtin_clz(v)
#endif

#ifndef AC_GPIC_REQUEST
#error AC_GPIC_REQUEST(set) is not defined.
#endif

typedef uint32_t ac_gpic_mask_t;

enum {
    AC_GPIC_PRIO_MAX = 31,
    AC_GPIC_PRIO_MIN = 0,
};

static inline unsigned int lg2(uint32_t v) {
    return 31 - AC_GPIC_CLZ(v);
}

//
// Fills bits lower than MSB with ones:
// i.e. 00001010 became 00001111,
//      00101110 became 00111111, etc.
// equivalent: (1 << (log2(x) + 1)) - 1
//
static inline uint32_t fill_bits(uint32_t v) {
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    return v;
}

struct ac_gpic_t {
    volatile uint32_t mask;
    volatile uint32_t pending;
    volatile uint32_t active;
};

static inline void ac_gpic_request(struct ac_gpic_t* pic, unsigned int vect) {
    const uint32_t new_pending = pic->pending | (UINT32_C(1) << vect);
    pic->pending = new_pending;
    
    if ((~pic->mask) & new_pending) {
        AC_GPIC_REQUEST(1);
    }
}

static inline uint32_t ac_gpic_mask(struct ac_gpic_t* pic, unsigned int prio) {
    const uint32_t mask = (((UINT32_C(1) << prio) - 1) << 1) | 1;
    const uint32_t old_mask = pic->mask;
    pic->mask = mask;
    AC_GPIC_REQUEST(0);

    return old_mask;
}

static inline void ac_gpic_unmask(struct ac_gpic_t* pic, uint32_t new_mask) {
    pic->mask = new_mask;
    
    if ((~new_mask) & pic->pending) {
        AC_GPIC_REQUEST(1);
    }
}

static inline unsigned int ac_gpic_start(struct ac_gpic_t* pic) {
    const uint32_t unmasked = (~pic->mask) & pic->pending;
    const unsigned int vect = lg2(unmasked);
    const uint32_t mask = UINT32_C(1) << vect;
    ac_gpic_mask(pic, vect);
    pic->pending &= ~mask;
    pic->active |= mask;

    return vect;
}

static inline void ac_gpic_done(struct ac_gpic_t* pic) {
    const uint32_t active = pic->active;
    const uint32_t ones_except_msb = fill_bits(active) >> 1;
    const uint32_t new_active = active & ones_except_msb;
    const uint32_t prev_mask = fill_bits(new_active);
    pic->active = new_active;
    ac_gpic_unmask(pic, prev_mask);
}

#endif

