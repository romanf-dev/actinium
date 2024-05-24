/** 
  ******************************************************************************
  *  @file   actinium.s
  *  @brief  Low-level interrupt-entry functions for the Actinium framework.
  *****************************************************************************/

.syntax unified
.cpu cortex-m4
.fpu softvfp
.thumb
.section .text

.global ac_kernel_start
.global ac_intr_entry
.global ac_svc_entry

.type ac_intr_entry, %function
ac_intr_entry:
    mrs r0, ipsr
    subs r0, r0, #16
    mrs r2, psp
    tst lr, #4
    ite eq
    moveq r1, sp
    movne r1, r2
    and r2, lr, #4
    lsr r2, r2, #2
    orr r1, r1, r2
    eor r1, r1, #1
    bl ac_intr_handler
    b exc_return

.type ac_svc_entry, %function
ac_svc_entry:
    lsr r1, lr, #2
    tst r1, #1
    bne goto_usr
    mov r1, sp
    msr psp, r1
    mov r1, #3
    msr control, r1
    dsb
    isb
    mov r1, #0
    msr basepri, r1
    mov r1, lr
    orr r1, r1, #4
    mov lr, r1
    bx lr
goto_usr:
    mrs r1, psp
    bl ac_svc_handler
    b exc_return

.type exc_return, %function
exc_return:
    eor r0, r0, #1
    ldr lr, =0xFFFFFFF1
    mrs r1, psp
    mov r2, sp
    bic r3, r0, #1
    tst r0, #1
    ite eq
    moveq r2, r3
    movne r1, r3
    mov sp, r2
    msr psp, r1
    and r0, r0, #1
    lsl r1, r0, #2
    lsl r0, r0, #3
    orr r0, r0, r1
    orr lr, lr, r0
    bx lr

.type ac_kernel_start, %function
ac_kernel_start:
    ldr r0, =_estack
    mov sp, r0
    svc 0
    b .

