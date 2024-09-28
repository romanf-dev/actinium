/** 
  ******************************************************************************
  *  @file   hal.c
  *  @brief  Low-level interrupt handler wrappers. These functions are called 
  *          by the assembly part of the HAL.
  *****************************************************************************/

#include <stdint.h>
#include <assert.h>
#include "ac_port.h"
#include "mg_port.h"
#include "ac_pic.h"

/* provided by higher layers */
extern void* ac_intr_handler(uint32_t vect, void* frame);
extern void* ac_trap_handler(uint32_t id);
extern void* ac_svc_handler(uint32_t arg, void* frame);
extern void ac_mei_handler(void);
extern void ac_mtimer_handler(void);

static struct pic_t g_pic;

void mg_interrupt_request(unsigned vect) {
    assert(vect != 0);
    mg_critical_section_enter();
    _pic_request(&g_pic, vect);
    mg_critical_section_leave();    
}

struct hal_frame_t* hal_intr_handler(struct hal_frame_t* frame) {
    const ac_pic_mask_t prev_mask = _pic_mask(&g_pic, AC_PIC_PRIO_MAX);
    mg_critical_section_leave();
    ac_mei_handler();
    mg_critical_section_enter();
    _pic_unmask(&g_pic, prev_mask);

    return frame;
}

struct hal_frame_t* hal_mtimer_handler(struct hal_frame_t* frame) {
    const ac_pic_mask_t prev_mask = _pic_mask(&g_pic, AC_PIC_PRIO_MAX);
    ac_mtimer_handler();
    _pic_unmask(&g_pic, prev_mask);

    return frame;
}

struct hal_frame_t* hal_trap_handler(uint32_t mcause) {
    mg_critical_section_leave();
    struct hal_frame_t* const frame = ac_trap_handler(mcause);
    mg_critical_section_enter();
    _pic_done(&g_pic);

    return frame;
}

/*
 * N.B. When returned frame is the same as parameter it means that called 
 * actors are completed synchronously so interrupt should be marked as
 * completed.
 */
struct hal_frame_t* hal_msi_handler(struct hal_frame_t* prev_frame) {
    const unsigned vect = _pic_start(&g_pic);
    assert(vect != 0);
    mg_critical_section_leave();
    struct hal_frame_t* const frame = ac_intr_handler(vect, prev_frame);
    mg_critical_section_enter();

    if (prev_frame == frame) {
        _pic_done(&g_pic);
    }

    return frame;
}

/*
 * N.B. Synchronous syscalls return the same frame as input, otherwise it
 * is assumed that the call is asynchronous and the current actor is
 * completed.
 */
struct hal_frame_t* hal_ecall_handler(unsigned arg, struct hal_frame_t* frame) {
    mg_critical_section_leave();
    struct hal_frame_t* const next_frame = ac_svc_handler(arg, frame);
    mg_critical_section_enter();

    if (frame != next_frame) {
        _pic_done(&g_pic);
    }

    return next_frame;
}

