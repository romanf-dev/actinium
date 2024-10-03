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

#define mg_port_clz(v) __builtin_clz(v)

#define mg_critical_section_enter() 
#define mg_critical_section_leave() 

extern void mg_interrupt_request(unsigned int vect);

#define pic_vect2prio(v) (v)
#define pic_interrupt_request(v) mg_interrupt_request(v)

#endif

