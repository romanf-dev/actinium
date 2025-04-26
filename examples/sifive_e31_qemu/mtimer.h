/** 
  ******************************************************************************
  *  @file   mtimer.h
  *  @brief  Helper functions for RISC-V mtimer.
  *****************************************************************************/

#ifndef MTIMER_H
#define MTIMER_H

#define mtimecmp ((volatile uint32_t*) 0x2004000)

static inline void mtimer_set_timecmp(uint64_t val) {
    mtimecmp[0]  = -1u;
    mtimecmp[1] = val >> 32;
    mtimecmp[0]  = val & 0xffffffffu;
}

static inline uint64_t mtimer_get_timecmp(void) {
    uint64_t temp = mtimecmp[1];
    temp <<= 32;
    return temp | mtimecmp[0];
}

static inline void mtimer_setup(void) {
    const uint64_t val = mtimer_get_timecmp();
    mtimer_set_timecmp(val + 30);
}

#endif

