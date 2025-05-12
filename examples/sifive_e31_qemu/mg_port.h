/** 
  * @file  mg_port.h
  * @brief Magnesium porting layer for generic RISCV32.
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

// 
// Sw-implemented CLZ as Zbb is not supported by E31.
//

static inline unsigned int mg_port_clz(uint32_t v) {
    uint32_t r = !v;
    uint32_t c = (v < 0x00010000u) << 4;
    r += c;
    v <<= c;
    c = (v < 0x01000000u) << 3;
    r += c;
    v <<= c;
    c = (v < 0x10000000u) << 2;
    r += c;
    v <<= c;
    c = (v >> 27) & 0x1e;
    r += (0x55afu >> c) & 3;
    return r;
}

#define MSIP_BASE ((volatile unsigned*) 0x2000000)
#define AC_GPIC_CLZ(v) mg_port_clz(v)
#define AC_GPIC_REQUEST(bit) (*(MSIP_BASE) = bit)
#include "arch/test/ac_gpic.h"

#define mg_critical_section_enter() { asm volatile ("csrc mstatus, 8"); }
#define mg_critical_section_leave() { asm volatile ("csrs mstatus, 8"); }
#define pic_vect2prio(v) (v)

extern struct ac_gpic_t g_pic;

static inline void pic_interrupt_request(unsigned cpu, unsigned vect) {
    mg_critical_section_enter();
    ac_gpic_request(&g_pic, vect);
    mg_critical_section_leave();    
}

#endif

