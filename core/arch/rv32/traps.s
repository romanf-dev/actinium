/** 
  ******************************************************************************
  *  @file    traps.s
  *  @brief   Low-level trap entry functions for the Actinium framework.
  *  @warning This code depends on interrupt frame defined in ac_port.h!
  *****************************************************************************/

.section .bss

/*
 * Callee-saved registers are preserved here before the switch to U-mode.
 * Task binaries may use these registers freely.
 */
kregs:
.long 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

.section .text

.global ac_kernel_start
.global ac_intr_entry
.global hal_pmp_update_entry3
.global hal_pmp_reprogram

.set SMALL_FRAME_SZ,(18*4)
.set FULL_FRAME_SZ,(33*4)
.set MIE_MASK,8
.set STACK_ALIGN_MASK,15
.set EXC_ID_MS,3
.set EXC_ID_MT,7
.set EXC_ID_EXTERNAL,11
.set EXC_ID_ECALL,8

/* 
 * N.B.
 * - mscratch holds the top of kernel stack when CPU is in U-mode and is zero
 *   in M-mode.
 * - a0/x10 may hold syscall parameter so it must not be overwritten until it
 *   is known that the current exception is not a syscall.
 */
ac_intr_entry:
    csrrw sp, mscratch, sp 
    bne sp, zero, from_umode
    csrrw sp, mscratch, sp
    addi sp, sp, -SMALL_FRAME_SZ
from_kmode:
    sw x1, 2*4(sp)
    sw x5, 3*4(sp)
    sw x6, 4*4(sp)
    sw x7, 5*4(sp)
    sw x10, 6*4(sp)
    sw x11, 7*4(sp)
    sw x12, 8*4(sp)
    sw x13, 9*4(sp)
    sw x14, 10*4(sp)
    sw x15, 11*4(sp)
    sw x16, 12*4(sp)
    sw x17, 13*4(sp)
    sw x28, 14*4(sp)
    sw x29, 15*4(sp)
    sw x30, 16*4(sp)
    sw x31, 17*4(sp)
    csrr x11, mepc
    sw x11, 4(sp)
    csrr x11, mstatus
    sw x11, 0(sp)
    j done
from_umode:
    addi sp, sp, -FULL_FRAME_SZ
    sw x3, 19*4(sp)
    sw x4, 20*4(sp)
    sw x8, 21*4(sp)
    sw x9, 22*4(sp)
    sw x18, 23*4(sp)
    sw x19, 24*4(sp)
    sw x20, 25*4(sp)
    sw x21, 26*4(sp)
    sw x22, 27*4(sp)
    sw x23, 28*4(sp)
    sw x24, 29*4(sp)
    sw x25, 30*4(sp)
    sw x26, 31*4(sp)
    sw x27, 32*4(sp)
    csrrw x8, mscratch, zero
    sw x8, 18*4(sp)
    la x27, kregs   /* load preserved kernel registers */
    lw x3, 0*4(x27)
    lw x4, 1*4(x27)
    lw x8, 2*4(x27)
    lw x9, 3*4(x27)
    lw x18, 4*4(x27)
    lw x19, 5*4(x27)
    lw x20, 6*4(x27)
    lw x21, 7*4(x27)
    lw x22, 8*4(x27)
    lw x23, 9*4(x27)
    lw x24, 10*4(x27)
    lw x25, 11*4(x27)
    lw x26, 12*4(x27)
    lw x27, 13*4(x27)
    j from_kmode
done:
    mv a1, sp
    li t0, STACK_ALIGN_MASK
    not t0, t0
    and sp, sp, t0

    /* a1 points to the saved frame here, sp is aligned */
    csrr t0, mcause
    srli t1, t0, 31
    beq t1, zero, exception
interrupt:
    mv a0, a1
    andi t0, t0, 15
    li t1, EXC_ID_EXTERNAL
    beq t0, t1, hwintr
    li t1, EXC_ID_MT    
    beq t0, t1, mtimer
    li t1, EXC_ID_MS
    beq t0, t1, swintr
exception:
    li t1, EXC_ID_ECALL      
    beq t0, t1, envcall
    csrr a0, mcause             /* the exception is not a syscall */
    mv s0, a1                   /* preserve the pointer to the frame */
    jal hal_trap_handler
    addi sp, s0, FULL_FRAME_SZ  /* skip the saved stack frame */
    j context_restore
hwintr:
    jal hal_intr_handler
    j context_restore
swintr:
    jal hal_msi_handler
    j context_restore
envcall:
    mv s0, a1                   /* preserve the pointer to the syscall frame */
    jal hal_ecall_handler
    beq a0, s0, skip_sp_adjust
    addi sp, s0, FULL_FRAME_SZ  /* skip the syscall frame when async call */
skip_sp_adjust:
    j context_restore
mtimer:
    jal hal_mtimer_handler

context_restore: /* a0 points to context, intrs are disabled */
    mv x31, a0
    lw t0, 0(x31)
    csrw mstatus, t0
    lw t0, 4(x31)
    csrw mepc, t0
    lw x1, 2*4(x31)
    lw x5, 3*4(x31)
    lw x6, 4*4(x31)
    lw x7, 5*4(x31)
    lw x10, 6*4(x31)
    lw x11, 7*4(x31)
    lw x12, 8*4(x31)
    lw x13, 9*4(x31)
    lw x14, 10*4(x31)
    lw x15, 11*4(x31)
    lw x16, 12*4(x31)
    lw x17, 13*4(x31)
    lw x28, 14*4(x31)
    lw x29, 15*4(x31)
    csrr x30, mstatus
    srli x30, x30, 11   /* MPP offset */
    andi x30, x30, 3    /* MPP mask */
    beq x30, zero, to_umode
    mv sp, x31
    lw x30, 16*4(sp)
    lw x31, 17*4(sp)
    addi sp, sp, SMALL_FRAME_SZ
    mret
to_umode:
    csrw mscratch, sp
    mv sp, x31
    la x31, kregs       /* preserve kernel registers using x31/x30 */
    sw x3, 0*4(x31)
    sw x4, 1*4(x31)
    sw x8, 2*4(x31)
    sw x9, 3*4(x31)
    sw x18, 4*4(x31)
    sw x19, 5*4(x31)
    sw x20, 6*4(x31)
    sw x21, 7*4(x31)
    sw x22, 8*4(x31)
    sw x23, 9*4(x31)
    sw x24, 10*4(x31)
    sw x25, 11*4(x31)
    sw x26, 12*4(x31)
    sw x27, 13*4(x31)
    lw x3, 19*4(sp)     /* 'large' frame part */
    lw x4, 20*4(sp)
    lw x8, 21*4(sp)
    lw x9, 22*4(sp)
    lw x18, 23*4(sp)
    lw x19, 24*4(sp)
    lw x20, 25*4(sp)
    lw x21, 26*4(sp)
    lw x22, 27*4(sp)
    lw x23, 28*4(sp)
    lw x24, 29*4(sp)
    lw x25, 30*4(sp)
    lw x26, 31*4(sp)
    lw x27, 32*4(sp)
    lw x30, 16*4(sp)
    lw x31, 17*4(sp)
    lw sp, 18*4(sp)
    mret

ac_kernel_start:
    csrs mie, 8 /* enable machine sw interrupts */
    csrs mstatus, MIE_MASK
wait:    
    wfi
    j wait

hal_pmp_update_entry3:
    csrw pmpcfg3, a0
    ret

hal_pmp_reprogram:
    lw a1, 0(a0)
    csrw pmpcfg0, a1
    lw a1, 4(a0)
    csrw pmpcfg1, a1
    lw a1, 8(a0)
    csrw pmpcfg2, a1
    lw a1, 12(a0)
    csrw pmpcfg3, a1
    lw a1, 16(a0)
    csrw pmpcfg4, a1
    ret

