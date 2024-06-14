/** 
  ******************************************************************************
  *  @file   actinium.s
  *  @brief  Low-level interrupt-entry functions for the Actinium framework.
  *          Here be dragons!
  *****************************************************************************/

.syntax unified
.cpu cortex-m4
.fpu softvfp
.thumb

.section .bss

/*
 * Since tasks are separate binaries they may or may not be AAPCS-compliant, so
 * we need to preserve kernel's high registers every time when we cross usermode
 * boundary. This is storage for that purpose.
 * Another issue is task's high registers. In principle each task may use them 
 * by its own way, and we cannot just push them each time on the stack (as 
 * regular RTOS do) since we can't trust user stack pointer.
 * Our strategy here is to read stack region address (set by trusted kernel)
 * directly from MPU and use the stack to save registers.
 * And yet more complexity: stacks are reused by different tasks, how can we 
 * store context there? The solution is that stacks are reused by tasks with
 * the same priority. Our stack will not be broken by another task until we
 * request blocking syscall. But blocking syscall means that task's function
 * is already returned so any registers are not needed anymore.
 */

hiregs:
.word 0,0,0,0,0,0,0,0

/*
 * When CPU's R4-R11 is loaded with kernel ones this field is 0. Otherwise it
 * points to task's stack when registers may be stored. So, if this field is
 * nonzero then 'hiregs' buffer contains valid kernel registers.
 */

target:
.word 0

.section .text

.global ac_kernel_start
.global ac_intr_entry
.global ac_svc_entry
.global ac_trap_entry

/*
 * Interrupts may interrupt anything, so both cases must be covered:
 * interruption of privileged and unprivileged code.
 */

.type ac_intr_entry, %function
ac_intr_entry:
    mrs     r0, psp
    tst     lr, #4
    ite     eq              // Use SP or PSP as frame pointer depending on LR.
    addeq   r1, sp, #1
    movne   r1, r0
    ldr     r0, =target
    ldr     r2, =hiregs
    cpsid   i
    ldr     r3, [r0]        // Load target and preserve it in R12.
    mov     r12, r3
    teq     r3, #0
    itttt   ne              // Switch high registers if needed.
    stmiane r3, { r4-r11 }
    ldmiane r2, { r4-r11 }
    movne   r3, #0          // Set target = 0.
    strne   r3, [r0]
    cpsie   i
    mrs     r0, ipsr        // Provide 1st argument, vector number.
    subs    r0, r0, #16
    bl      ac_intr_handler
    b       exc_return      // R12 is preserved so we'll restore hiregs later.

/*
 * In the current design there is (very optimistic) assumption that exceptions
 * may only be thrown from usermode.
 */

.type ac_trap_entry, %function
ac_trap_entry:
    mrs     r1, psp
    ldr     r0, =target
    ldr     r2, =hiregs
    cpsid   i               // Switch hiregs if needed.
    ldr     r3, [r0]        
    teq     r3, #0
    itttt   ne
    stmiane r3, { r4-r11 }
    ldmiane r2, { r4-r11 }
    movne   r3, #0          // Set target = 0.
    strne   r3, [r0]
    cpsie   i
    mrs     r0, ipsr
    bl      ac_trap_handler
    b       exc_return

.type ac_svc_entry, %function
ac_svc_entry:
    tst     lr, #4          // First call from main is a special case.
    bne     syscall         // It is used to enable unprivileged threads.
    mov     r0, sp          // SP is MSP and points to pushed hw frame here.
    msr     psp, r0         // Set PSP for return.
    sub     r0, #32         // Make room for idle-task's hiregs on top of
    mov     sp, r0          // interrupt stack. MSP points just below idle's
    mov     r0, #0          // stack.
    msr     basepri, r0
    mov     r0, #3
    msr     control, r0
    orr     lr, lr, #4
    dsb
    isb
    bx lr
syscall:                    // Syscalls are always from usermode, no need 
    ldr     r1, =target     // to check LR.
    ldr     r2, =hiregs
    ldr     r3, [r1]
    cpsid   i               // Switch hiregs unconditionally.
    stmia   r3, { r4-r11 }
    ldmia   r2, { r4-r11 }
    mov     r3, #0
    str     r3, [r1]
    cpsie   i
    mrs     r1, psp
    bl      ac_svc_handler
    b       exc_return

/*
 * The most complex part. We came here with R0 pointing to the stack frame.
 * bit 0 value is 0 if the target mode is unprivileged and 1 otherwise.
 */

.type exc_return, %function
exc_return:
    ldr     lr, =0xFFFFFFF1 // Return to handler. Will be adjusted later.
    mrs     r1, psp
    tst     r0, #1
    ittee   ne              // Set either MSP or PSP.
    bicne   r0, r0, #1
    movne   sp, r0
    moveq   r1, r0
    orreq   lr, lr, #12     // LR = 0xfffffffd, 'return to thread using PSP'.
    msr     psp, r1
    mov     r1, #2          // Stack region number in the MPU.
    mov     r2, r12         // Possibly preserved target.
    tst     lr, #4
    itttt   ne
    ldrne   r3, =0xe000ed98 // Read base addr of region 2 if return to thread.
    strne   r1, [r3]
    addne   r3, #4
    ldrne   r2, [r3]
    bic     r2, #15
    ldr     r0, =hiregs
    ldr     r1, =target
    cpsid   i
    teq     r2, #0          // R2 would be nonzero if either return to thread 
    ittt    ne              // or preserved R12 is nonzero.
    stmiane r0, { r4-r11 }
    strne   r2, [r1]
    ldmiane r2, { r4-r11 }
    cpsie   i               // Another interrupts may happen here!
    bx      lr

/*
 * Called from main while still in privileged mode on MSP stack.
 * Some actors are already activated and pending, so we can't lower basepri
 * mask before enabling usermode. But after usermode is enabled via control
 * reg we will lose access to system registers including basepri.
 * This is the reason why we need svc here and the special case.
 */

.type ac_kernel_start, %function
ac_kernel_start:
    ldr     r0, =_estack
    mov     sp, r0
    svc     0
    b       .

