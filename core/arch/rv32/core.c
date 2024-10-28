/** 
  ******************************************************************************
  *  @file   core.c
  *  @brief  Low-level interrupt handler wrappers. These functions are called 
  *          by the assembly part of the HAL.
  *****************************************************************************/

#include <stdint.h>
#include <assert.h>
#include "ac_port.h"
#include "mg_port.h"
#include "arch/test/ac_gpic.h"
#include "actinium.h"

/* provided by higher layers */
extern void ac_port_mei_handler(void);
extern void ac_port_mtimer_setup(void);

/* sw-implemented fake 'interrupt controller' simulated in MS interrupt. */
static struct ac_gpic_t g_pic;

void mg_interrupt_request(unsigned vect) {
    assert(vect != 0);
    mg_critical_section_enter();
    ac_gpic_request(&g_pic, vect);
    mg_critical_section_leave();    
}

struct ac_port_frame_t* ac_port_intr_handler(struct ac_port_frame_t* frame) {
    const ac_gpic_mask_t prev_mask = ac_gpic_mask(&g_pic, AC_GPIC_PRIO_MAX);
    mg_critical_section_leave();
    ac_port_mei_handler();
    mg_critical_section_enter();
    ac_gpic_unmask(&g_pic, prev_mask);

    return frame;
}

struct ac_port_frame_t* ac_port_mtimer_handler(struct ac_port_frame_t* frame) {
    const ac_gpic_mask_t prev_mask = ac_gpic_mask(&g_pic, AC_GPIC_PRIO_MAX);
    ac_port_mtimer_setup();
    mg_critical_section_leave();
    ac_context_tick();
    mg_critical_section_enter();
    ac_gpic_unmask(&g_pic, prev_mask);

    return frame;
}

struct ac_port_frame_t* ac_port_trap_handler(uint32_t mcause) {
    mg_critical_section_leave();
    struct ac_port_frame_t* const frame = ac_actor_exception();
    mg_critical_section_enter();
    ac_gpic_done(&g_pic);

    return frame;
}

/*
 * N.B. When returned frame is the same as parameter it means that called 
 * actors are completed synchronously so interrupt should be marked as
 * completed.
 */
struct ac_port_frame_t* ac_port_msi_handler(struct ac_port_frame_t* prev) {
    const unsigned vect = ac_gpic_start(&g_pic);
    assert(vect != 0);
    mg_critical_section_leave();
    struct ac_port_frame_t* const frame = _ac_intr_handler(vect, prev);
    mg_critical_section_enter();

    if (prev == frame) {
        ac_gpic_done(&g_pic);
    }

    return frame;
}

/*
 * N.B. Synchronous syscalls return the same frame as input, otherwise it
 * is assumed that the call is asynchronous and the current actor is
 * completed.
 */
struct ac_port_frame_t* ac_port_ecall_handler(
    unsigned arg, 
    struct ac_port_frame_t* frame
) {
    mg_critical_section_leave();
    struct ac_port_frame_t* const next_frame = _ac_svc_handler(arg, frame);
    mg_critical_section_enter();

    if (frame != next_frame) {
        ac_gpic_done(&g_pic);
    }

    return next_frame;
}

