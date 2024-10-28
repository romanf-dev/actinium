/** 
  ******************************************************************************
  *  @file   core.c
  *  @brief  Hosted HAL for Actinium execution inside the host OS thread.
  *          Intended to be used for tests but also may be useful for 
  *          prototyping.
  *****************************************************************************/

#include <stdint.h>
#include <assert.h>
#include <setjmp.h>
#include "ac_port.h"
#include "mg_port.h"
#include "ac_gpic.h"
#include "actinium.h"

/* sw-implemented interrupt controller and a pending interrupt flag */
static struct ac_gpic_t g_pic;
static unsigned g_req;

/*
 * External function used by RV32 virtual controller to set MSIP bit indicating
 * presence of unmasked pending interrupt. In this HAL MSIP bit itself is also
 * implemented in software and must be checked manually. Interrupt request
 * flag should eventually result in sofwtare interrupt handler call.
 */
void ac_gpic_req_set(unsigned bit) {
    g_req = bit;
}

void mg_interrupt_request(unsigned vect) {
    assert(vect != 0);
    ac_gpic_request(&g_pic, vect);
}

void* _ac_syscall(unsigned arg);

/*
 * Asynchronous actor preemption/activation handler.
 * There may be two kinds of actors privileged and unprivileged ones.
 * Privileged actors are called inside the ac_intr_handler function, so 
 * returned frame is the same as the argument passed. When returned
 * value does not match the parameter it means that preemption occurs. In that
 * case the context is saved using setjmp and the new actor is activated.
 * Returned frame is used just for parameter passing.
 */
void ac_port_swi_handler(void) {
    const unsigned vect = ac_gpic_start(&g_pic);
    assert(vect != 0); 

    /*
     * Create the new 'interrupt frame' on stack and call the kernel.
     * A pointer to temp may be saved inside the function in case of preemption
     * as 'preempted context'.
     */
    struct ac_port_frame_t temp;
    struct ac_port_frame_t* const frame = _ac_intr_handler(vect, &temp);
    
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
                _ac_syscall(syscall);
            }
        } else {

            /*
             * This branch is executed when the actor is completed.
             */
            ac_gpic_done(&g_pic);
        }
    } else {
        ac_gpic_done(&g_pic); 
    }

    /*
     * Actor completion may unblock some other actors.
     */
    if (g_req) {
        ac_port_swi_handler();
    }
}

/*
 * This function is used inside an actor to emulate a syscall.
 */
void* _ac_syscall(unsigned arg) {

    /*
     * Create the 'interrupt frame'. In case of synchronous calls this is used
     * for parameter passing only. Asynchronous calls do not use this frame as
     * they return to the caller.
     */
    struct ac_port_frame_t temp;
    struct ac_port_frame_t* const next_frame = _ac_svc_handler(arg, &temp);
    void* result = 0;

    /*
     * Synchronous syscalls may activate another actors i.e. by posting mesage
     * into a channel.
     */
    if (g_req) {        
        ac_port_swi_handler();
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
void ac_port_trap_handler(uint32_t id) {
    struct ac_port_frame_t* const frame = ac_actor_exception();
    longjmp(frame->context, 0);
}

