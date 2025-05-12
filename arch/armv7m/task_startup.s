/*
 *  @file   task_startup.s
 *  @brief  Low-level syscall and startup code for Actinium usermode tasks.
 */

.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

.section .startup
.type startup, %function
startup:
    teq lr, #0            /* Nonzero LR means 'cold restart' with bss reinit. */
    beq task_run

    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_etext
    movs r3, #0
    b data_init

data_copying:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

data_init:
    adds r4, r0, r3
    cmp r4, r1
    bcc data_copying

    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
    b start_bss_init

bss_zeroing:
    str  r3, [r1]
    adds r1, r1, #4

start_bss_init:
    cmp r1, r2
    bcc bss_zeroing

    bl _ac_init_once      /* Weak func. May be used by libc. */
    mov r0, #0            /* Zero message at the first call. */

task_run:
    bl main
    svc 0                 /* main return value is the syscall arg. */
    b task_run

.global _ac_syscall
.section .text
.type _ac_syscall, %function
_ac_syscall:
    svc 0
    bx lr

.weak _ac_init_once
.type _ac_init_once, %function
_ac_init_once:
    bx lr

