/** 
  ******************************************************************************
  *  @file   hal.c
  *  @brief  Hosted HAL for Actinium execution inside the host OS thread.
  *          Intended to be used for tests but also may be useful for 
  *          prototyping purposes.
  *****************************************************************************/

#include <stdint.h>
#include <assert.h>
#include <setjmp.h>
#include "ac_port.h"
#include "mg_port.h"
#include "ac_pic.h"
#include "actinium.h"

/* provided by higher layers */
extern void* ac_intr_handler(uint32_t vect, void* frame);
extern void* ac_trap_handler(uint32_t id);
extern void* ac_svc_handler(uint32_t arg, void* frame);

void* hal_syscall_handler(unsigned arg);

/* sw-implemented interrupt controller and a pending interrupt flag */
static struct pic_t g_pic;
static unsigned g_req;

/*
 * External function used by RV32 virtual controller to set MSIP bit indicating
 * presence of unmasked pending interrupt. In this HAL MSIP bit itself is also
 * implemented in software and must be checked manually. Interrupt request
 * flag should eventually result in sofwtare interrupt handler call.
 */
void ac_msip_set(unsigned bit) {
    g_req = bit;
}

void mg_interrupt_request(unsigned vect) {
    assert(vect != 0);
    _pic_request(&g_pic, vect);
}

/*
 * Asynchronous actor preemption/activation handler.
 * There may be two kinds of actors privileged and unprivileged ones.
 * Privileged actors are called inside the ac_intr_handler function, so 
 * returned frame is the same as the argument passed. When returned
 * value does not match the parameter it means that preemption occurs. In that
 * case the context is saved using setjmp and the new actor is activated.
 * Returned frame is used just for parameter passing.
 */
void hal_swi_handler(void) {
    const unsigned vect = _pic_start(&g_pic);
    assert(vect != 0); 

    /*
     * Create the new 'interrupt frame' on stack and call the kernel.
     * A pointer to temp may be saved inside the function in case of preemption
     * as 'preempted context'.
     */
    struct hal_frame_t temp;
    struct hal_frame_t* const frame = ac_intr_handler(vect, &temp);
    
    if (&temp != frame) {
        void* const arg = frame->arg;
        uint32_t (* const func)(void*) = frame->func;

        /*
         * Preemption case. It is assumed that actor will exit via either 
         * async syscall inside its function or exception. Both cases lead to
         * longjmp and execution of the 'else' branch.
         */
        if (!setjmp(temp.context)) {
            for (;;) {
                const uint32_t syscall = func(arg);
                hal_syscall_handler(syscall);
            }
        } else {

            /*
             * This branch is executed when the actor is completed.
             */
            _pic_done(&g_pic);
        }
    } else {
        _pic_done(&g_pic); 
    }

    /*
     * Actor completion may unblock some other actors.
     */
    if (g_req) {
        hal_swi_handler();
    }
}

/*
 * This function is used inside an actor to emulate a syscall.
 */
void* hal_syscall_handler(unsigned arg) {

    /*
     * Create the 'interrupt frame'. In case of synchronous calls this is used
     * for parameter passing only. Asynchronous calls do not use this frame as
     * they return to the caller.
     */
    struct hal_frame_t temp;
    struct hal_frame_t* const next_frame = ac_svc_handler(arg, &temp);
    void* result = 0;

    /*
     * Synchronous syscalls may activate another actors i.e. by posting mesage
     * into a channel.
     */
    if (g_req) {        
        hal_swi_handler();
    }

    /*
     * Returning non-local frame means asynchronous call and actor completion.
     * Otherwise extract the syscall return from the frame.
     */
    if (&temp != next_frame) {
        longjmp(next_frame->context, 0);
    } else {
        result = temp.arg;
    }

    return result;
}

/*
 * Exceptions are always synchronous and cause return to the interrupted actor.
 */
void hal_trap_handler(uint32_t id) {
    struct hal_frame_t* const frame = ac_trap_handler(id);
    longjmp(frame->context, 0);
}

void* ac_intr_handler(uint32_t vect, void* frame) {
    return _ac_intr_handler(vect, frame);
}

void* ac_svc_handler(uint32_t arg, void* frame) {
    return _ac_svc_handler(arg, frame);
}

void* ac_trap_handler(uint32_t id) {
    return ac_actor_exception();
}

