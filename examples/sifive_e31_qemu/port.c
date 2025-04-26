/** 
  ******************************************************************************
  *  @file   port.c
  *  @brief  Functions required by RISC-V platform.
  *****************************************************************************/

#include <stdint.h>
#include <assert.h>
#include "actinium.h"
#include "mtimer.h"

#define MSIP_BASE ((void*) 0x2000000)
#define MCAUSE_ENVCALL 8

/* sw-implemented fake 'interrupt controller' simulated in MS interrupt. */
struct ac_gpic_t g_pic;

/* required by the porting layer */
void ac_gpic_req_set(unsigned req) {
    volatile uint32_t* const msip = MSIP_BASE;
    *msip = req;
}

/* This port does not handle hardware interrupts except mtimer/msoftirq. */
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

/*
 * N.B. Synchronous syscalls return the same frame as input, otherwise it
 * is assumed that the call is asynchronous and the current actor is
 * completed.
 */
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

/*
 * N.B. When returned frame is the same as parameter it means that called 
 * actors are completed synchronously so interrupt should be marked as
 * completed.
 */
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

