/*
 *  @file   port.c
 *  @brief  Functions required by RISC-V platform.
 */

#include <stdint.h>
#include <assert.h>
#include "actinium.h"
#include "mtimer.h"

#define AC_GPIC_CLZ(v) mg_port_clz(v)
#define AC_GPIC_REQUEST(bit) (*((volatile unsigned*) 0x2000000) = bit)
#include "arch/test/ac_gpic.h"

#define MCAUSE_ENVCALL 8

//
// E31 doesn't support CLZ, so implement it manually.
//
unsigned int mg_port_clz(uint32_t v) {
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

//
// sw-implemented fake 'interrupt controller' simulated in MS interrupt.
//
static struct ac_gpic_t g_pic;

unsigned pic_vect2prio(unsigned vect) {
    return vect;
}

void pic_interrupt_request(unsigned cpu, unsigned vect) {
    mg_critical_section_enter();
    ac_gpic_request(&g_pic, vect);
    mg_critical_section_leave();    
}

// This port does not handle hardware interrupts except mtimer/msoftirq.
struct ac_port_frame_t* ac_port_mei_handler(struct ac_port_frame_t* frame) {
    for (;;);
}

struct ac_port_frame_t* ac_port_mtimer_handler(struct ac_port_frame_t* frame) {
    const ac_gpic_mask_t prev_mask = ac_gpic_mask(&g_pic, AC_GPIC_PRIO_MAX);
    mtimer_setup();
    mg_critical_section_leave();
    ac_context_tick();
    mg_critical_section_enter();
    ac_gpic_unmask(&g_pic, prev_mask);

    return frame;
}

//
// N.B. Synchronous syscalls return the same frame as input, otherwise it
// is assumed that the call is asynchronous and the current actor is
// completed.
//

struct ac_port_frame_t* ac_port_trap_handler(
    struct ac_port_frame_t* frame,
    uint32_t mcause
) {
    mg_critical_section_leave();
    const uint32_t syscall = frame->r[REG_A0];
    struct ac_port_frame_t* const next_frame = (mcause == MCAUSE_ENVCALL) ?
        (frame->pc += sizeof(uint32_t)), _ac_svc_handler(syscall, frame):
        ac_actor_exception();
    mg_critical_section_enter();

    if (frame != next_frame) {
        ac_gpic_done(&g_pic);
    }

    return next_frame;
}

//
// N.B. When returned frame is the same as parameter it means that called 
// actors are completed synchronously so interrupt should be marked as
// completed.
//

struct ac_port_frame_t* ac_port_msi_handler(struct ac_port_frame_t* prev) {
    const unsigned vect = ac_gpic_start(&g_pic);
    mg_critical_section_leave();
    struct ac_port_frame_t* const frame = _ac_intr_handler(vect, prev);
    mg_critical_section_enter();

    if (prev == frame) {
        ac_gpic_done(&g_pic);
    }

    return frame;
}

