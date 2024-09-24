/** 
  * @file  mg_port.h
  * @brief Magnesium porting layer for generic RISCV32.
  * License: Public domain. The code is provided as is without any warranty.
  */
#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable asm functions. 
#endif

#if !defined MG_PRIO_MAX
#define MG_PRIO_MAX 8
#endif 

#if !defined MG_TIMERQ_MAX
#define MG_TIMERQ_MAX 10
#endif

/* 
 * clz is implemented in software using binary search to find the MSB.
 * @warning this implementation assumes that argument is nonzero!
 */
static inline unsigned int mg_port_clz(uint32_t v) {
    register unsigned int r;
    register unsigned int shift;

    r =     (v > 0xFFFF) << 4; v >>= r;
    shift = (v > 0xFF  ) << 3; v >>= shift; r |= shift;
    shift = (v > 0xF   ) << 2; v >>= shift; r |= shift;
    shift = (v > 0x3   ) << 1; v >>= shift; r |= shift;
                                            r |= (v >> 1);    
    return (31 - r);
}

#define mg_critical_section_enter() { asm volatile ("csrc mstatus, 8"); }
#define mg_critical_section_leave() { asm volatile ("csrs mstatus, 8"); }

/* interrupt request is platform-specific, use external functions */
extern void mg_interrupt_request(unsigned int vect);
extern unsigned int mg_interrupt_prio(unsigned int vect);

#define pic_vect2prio(v) mg_interrupt_prio(v)
#define pic_interrupt_request(v) mg_interrupt_request(v)

#endif

