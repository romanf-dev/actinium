/** 
  ******************************************************************************
  *  @file   ac_pic.h
  *  @brief  Programmable Interrupt Controller (PIC) implementation.
  *  
  *  Design description:
  *  This is a pure software implementation compatible with any interrupt
  *  controller. The HAL maintains 31 'vectors' available for actor execution.
  *  These vectors are represented as three dwords: masked, pending, active.
  *  Machine software interrupt is used as a workhorse for the preemption: 
  *  whenever there is unmasked vector (e.g. by set_pending) the MSIP is set 
  *  and machine software interrupt is triggered. Pending bits are reset upon 
  *  entering sw interrupt handler. Within handler processing vectors with
  *  lower priority remain masked so the actor cannot preempt itself.
  *  For simplicity, priority 0 is reserved because software implementation of
  *  log2 function return identical values for 0 and 1 argument.
  *****************************************************************************/

#ifndef AC_PIC_H
#define AC_PIC_H

#include "mg_port.h"

/* sets MSIP bit in mip register */
extern void ac_msip_set(unsigned int bit);

typedef uint32_t ac_pic_mask_t;

enum {
    AC_PIC_PRIO_MAX = 31,
    AC_PIC_PRIO_MIN = 1,
};

/*
 * N.B. v must be nonzero.
 */
static inline unsigned int lg2(uint32_t v) {
    return 31 - mg_port_clz(v);
}

/*
 * Fills bits lower than MSB with ones:
 * i.e. 00001010 became 00001111,
 *      00101110 became 00111111, etc.
 * equivalent: (1 << (log2(x) + 1)) - 1
 */
static inline uint32_t fill_bits(uint32_t v) {
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v;
}

struct pic_t {
    volatile uint32_t mask;
    volatile uint32_t pending;
    volatile uint32_t active;
};

static inline void _pic_request(struct pic_t* pic, unsigned int vect) {
    const uint32_t new_pending = pic->pending | (UINT32_C(1) << vect);
    pic->pending = new_pending;
    
    if ((~pic->mask) & new_pending) {
        ac_msip_set(1);
    }
}

static inline uint32_t _pic_mask(struct pic_t* pic, unsigned int prio) {
    const uint32_t mask = (((UINT32_C(1) << prio) - 1) << 1) | 1;
    const uint32_t old_mask = pic->mask;
    pic->mask = mask;
    ac_msip_set(0);

    return old_mask;
}

static inline void _pic_unmask(struct pic_t* pic, uint32_t new_mask) {
    pic->mask = new_mask;
    
    if ((~new_mask) & pic->pending) {
        ac_msip_set(1);
    }
}

static inline unsigned int _pic_start(struct pic_t* pic) {
    const uint32_t unmasked = (~pic->mask) & pic->pending;
    const unsigned int vect = lg2(unmasked);
    const uint32_t mask = UINT32_C(1) << vect;
    _pic_mask(pic, vect);
    pic->pending &= ~mask;
    pic->active |= mask;

    return vect;
}

static inline void _pic_done(struct pic_t* pic) {
    const uint32_t active = pic->active;
    const uint32_t ones_except_msb = fill_bits(active) >> 1;
    const uint32_t new_active = active & ones_except_msb;
    const uint32_t prev_mask = fill_bits(new_active);
    pic->active = new_active;
    _pic_unmask(pic, prev_mask);
}

#endif

