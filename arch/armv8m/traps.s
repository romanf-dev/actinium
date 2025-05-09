/** 
  ******************************************************************************
  *  @file   traps.s
  *  @brief  Low-level interrupt-entry functions for the Actinium framework.
  *          Here be dragons!
  *****************************************************************************/

.syntax unified
.cpu cortex-m4
.fpu softvfp
.thumb

.set SHCSR,0xe000ed24
.set MPU_RNR,0xe000ed98

.global ac_kernel_start
.global ac_port_intr_entry
.global ac_port_svc_entry
.global ac_port_trap_entry
.global ac_port_idle_text

.section .text

/*
 * Interrupts may interrupt anything, so both cases must be covered:
 * interruption of privileged and unprivileged code.
 * The interrupt frame is splitted into two parts which are saved to
 * different stacks. Hardware-supplied frame is always pushed into the
 * current stack (PSP), but saving high registers into the same location
 * is unreliable as we cannot trust user stack pointer so the rest of the
 * frame is pushed into MSP. As handlers are always stacked this is not an
 * issue but requires some care in the low-level code.
 * LSB of the frame pointer is also used as a flag indicating return mode:
 * 0 - usermode frame, 1 - kernel-mode frame. This bit is also selects
 * the appropriate stack pointer on return either MSP or PSP.
 */

.type ac_port_intr_entry, %function
ac_port_intr_entry:
    stmdb   sp!, { r4-r11 } // Save callee-saved registers on MSP always.
    mrs     r0, psp         // MRS can't be in IT block, read for further use.
    tst     lr, #4          // Is usermode interrupted?
    ite     eq              //
    addeq   r1, sp, #1      // No, get the frame ptr from MSP plus mode flag.
    movne   r1, r0          // Yes, frame in previously read PSP.
    mrs     r0, ipsr        // Provide 1st argument, vector number.
    subs    r0, #16         // Get interrupt vector by exception id.
    movs    r4, r1          // Preserve the frame ptr in callee-saved reg.
    bl      ac_intr_handler // Get the next frame in R0.
    cmp     r0, r4          // Is new frame allocated?
    it      ne
    subsne  sp, #32         // Yes, simulate STMDB with high regs.
    b       exc_return

/*
 * In the current design there is (very optimistic) assumption that exceptions
 * may only be thrown from usermode.
 * Since it is possible that multiple exceptions are pending they are cleared
 * to prevent exceptions to occur in the next actor.
 * The high part of the frame is not saved and it is expected that the handler
 * will return the next stacked frame that matches the high regs which are
 * already on the stack.
 */

.type ac_port_trap_entry, %function
ac_port_trap_entry:
    ldr     r0, =SHCSR
    ldr     r1, [r0]
    bfc     r1, #12, #4
    str     r1, [r0]
    dsb
    isb
    mrs     r0, ipsr
    bl      ac_trap_handler
    b       exc_return

/*
 * Syscall handler. Syscalls are allowed from usermode only so no need to 
 * check the LR. If the handler returns the same frame is its input it
 * means that this call is synchronous.
 * Syscall exception must have priority less than other faults so they
 * will take priority in case of errors during the stacking.
 */

.type ac_port_svc_entry, %function
ac_port_svc_entry:
    tst     lr, #4          // First call from main is a special case.
    beq     enable_umode    // It is used to enable unprivileged threads.
    stmdb   sp!, { r4-r11 } // Save high registers on the MSP stack.
    mrs     r1, psp         // Read the frame pointer.
    ldr     r0, [r1]        // Read syscall argument in r0 from the frame.
    movs    r4, r1          // Preserve the frame ptr in callee-saved reg. 
    bl      ac_svc_handler  // Get the next frame.
    cmp     r0, r4          // Is this call is synchronous (same frame)?
    it      ne
    addsne  sp, #32         // No, skip high registers pushed by STMDB.
    b       exc_return    

/*
 * We came here with R0 pointing to the stack frame. The high registers
 * for the next actor are already on to of the MSP.
 */

exc_return:
    mvn     lr, #14         // 0xfffffff1 = return to handler. Adjusted later.
    mrs     r1, psp         // Pre-read PSP as mrs isn't allows in IT block.
    tst     r0, #1          // Return to handler mode?
    ittee   ne
    bicne   r0, #1          // Yes, reset the flag and obtain the next MSP.
    movne   sp, r0          // Set the MSP.
    moveq   r1, r0          // No, return to thread, overwrite PSP in r1.
    orreq   lr, #12         // LR = 0xfffffffd, 'return to thread using PSP'.
    msr     psp, r1         // Update the PSP.
    ldmia   sp!, { r4-r11 } // Load the high registers of the next actor.
    bx      lr              // Unstacking.

/*
 * Called just once from the idle thread.
 */

enable_umode:
    mov     r0, sp          // SP is MSP and points to pushed hw frame here.
    msr     psp, r0         // Set PSP for return.
    movs    r0, #0
    msr     basepri, r0
    movs    r0, #3
    msr     control, r0
    orr     lr, #4
    dsb
    isb
    bx lr

/*
 * Called from main while still in privileged mode on MSP stack.
 * Some actors are already activated and pending, so we can't lower basepri
 * mask before enabling usermode. But after usermode is enabled via control
 * reg we will lose access to system registers including basepri.
 * This is the reason why we need svc here and the special case.
 *
 * +----------------------+ <-- initial idle's stack top before SVC
 * | stacked idle's frame |
 * +----------------------+ <-- pointer to stacked frame inside SVC handler
 * | kernel-mode stack    |     this is also the low MPU boundary for the 
 * +----------------------+     idle stack and the top of the kernel stack
 *            V             <-- kernel stack grow direction
 */

.balign 32
ac_port_idle_text:
.type ac_kernel_start, %function
ac_kernel_start:
    ldr     r0, =MPU_RNR
    movs    r1, #2          // Read base addr of region 2 (stack).
    str     r1, [r0, #0]    // Set RNR.
    ldr     r1, [r0, #4]    // Read RBAR.
    bic     r1, #31         // Reset attribute bits.
    adds    r1, #32         // Add frame size = top of stack.
    mov     sp, r1
    svc     0
    b       .

