/*
 * @file  mg_port.h
 * @brief Magnesium porting layer for hosted test environment.
 */

#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable builtins. 
#endif

#if !defined MG_PRIO_MAX
#define MG_PRIO_MAX 31
#endif 

#if !defined MG_TIMERQ_MAX
#define MG_TIMERQ_MAX 10
#endif

#define mg_port_clz(v) __builtin_clz(v)

#define mg_critical_section_enter() 
#define mg_critical_section_leave() 

#define pic_vect2prio(v) (v)
extern void pic_interrupt_request(unsigned cpu, unsigned vect);

#endif

