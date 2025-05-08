/** 
  * @file  mg_port.h
  * @brief Magnesium porting layer for hosted test environment.
  * License: Public domain. The code is provided as is without any warranty.
  */
#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable builtins. 
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

extern unsigned g_req;
#define AC_GPIC_REQUEST(set) (g_req = (set))

#include "ac_gpic.h"
extern struct ac_gpic_t g_pic;

#define pic_vect2prio(v) (v)
#define pic_interrupt_request(cpu, vect) ac_gpic_request(&g_pic, vect)

#endif

