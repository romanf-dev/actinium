/** 
  ******************************************************************************
  *  @file    traps.s
  *  @brief   Low-level trap entry functions for the Actinium framework.
  *  @warning This code depends on interrupt frame defined in ac_port.h!
  *****************************************************************************/

.extern ac_port_msi_handler
.extern ac_port_mtimer_handler
.extern ac_port_intr_handler

.global ac_kernel_start
.global ac_port_intr_entry
.global ac_pmp_update_entry3
.global ac_pmp_reprogram

.set SMALL_FRAME_SZ,(18*4)
.set FULL_FRAME_SZ,(33*4)
.set MSTATUS_MIE,8
.set MIE_MSIE,8
.set MIE_MTIE,0x80
.set IDLE_STACK_SIZE,256
.set STACK_ALIGN_MASK,15

.section .rodata
.align 4

internal_vectors:
.word       0
.word       0
.word       0
.word       ac_port_msi_handler
.word       0
.word       0
.word       0
.word       ac_port_mtimer_handler
.word       0
.word       0
.word       0
.word       ac_port_intr_handler

/*
 * Callee-saved registers are preserved here before the switch to U-mode.
 * Task binaries may use these registers freely.
 */
.section .bss

kregs:
.long 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

.section .text

/* 
 * mscratch = top of kernel stack when CPU is in U-mode and is zero in M-mode.
 */
ac_port_intr_entry:
    csrrw   sp, mscratch, sp
    bnez    sp, from_umode          /* SP = kernel stack if prev mode is U */
    csrrw   sp, mscratch, zero      /* switch SP back is prev mode is M */
    addi    sp, sp, -SMALL_FRAME_SZ
    j       save_frame
from_umode:
    addi    sp, sp, -FULL_FRAME_SZ
    sw      s11, 32*4(sp)           /* save persistent user regs on kstack */
    sw      s10, 31*4(sp)
    sw      s9,  30*4(sp)
    sw      s8,  29*4(sp)
    sw      s7,  28*4(sp)
    sw      s6,  27*4(sp)
    sw      s5,  26*4(sp)
    sw      s4,  25*4(sp)
    sw      s3,  24*4(sp)
    sw      s2,  23*4(sp)
    sw      s1,  22*4(sp)
    sw      s0,  21*4(sp)
    sw      tp,  20*4(sp)
    sw      gp,  19*4(sp)
    csrrw   s0, mscratch, zero      /* s0 = top of user stack */
    sw      s0,  18*4(sp)
    la      s11, kregs              /* load preserved kernel registers */
    lw      gp,  0*4(s11)
    lw      tp,  1*4(s11)
    lw      s0,  2*4(s11)
    lw      s1,  3*4(s11)
    lw      s2,  4*4(s11)
    lw      s3,  5*4(s11)
    lw      s4,  6*4(s11)
    lw      s5,  7*4(s11)
    lw      s6,  8*4(s11)
    lw      s7,  9*4(s11)
    lw      s8,  10*4(s11)
    lw      s9,  11*4(s11)
    lw      s10, 12*4(s11)
    lw      s11, 13*4(s11)
save_frame:                         /* save volatile registers */
    sw      t6, 17*4(sp)
    sw      t5, 16*4(sp)
    sw      t4, 15*4(sp)
    sw      t3, 14*4(sp)
    sw      a7, 13*4(sp)
    sw      a6, 12*4(sp)
    sw      a5, 11*4(sp)
    sw      a4, 10*4(sp)
    sw      a3, 9*4(sp)
    sw      a2, 8*4(sp)
    sw      a1, 7*4(sp)
    sw      a0, 6*4(sp)
    sw      t2, 5*4(sp)
    sw      t1, 4*4(sp)
    sw      t0, 3*4(sp)
    sw      ra, 2*4(sp)
    csrr    t0, mepc
    csrr    t1, mstatus
    sw      t0, 1*4(sp)
    sw      t1, 0*4(sp)
    li      t0, STACK_ALIGN_MASK
    not     t0, t0
    mv      a0, sp                  /* pointer to saved frame as 2nd arg */
    and     sp, sp, t0
    csrr    a1, mcause
    bgez    a1, exception

interrupt:
    slli    a1, a1, 2
    la      t0, internal_vectors
    add     t0, t0, a1
    lw      t0, (t0)
    jalr    t0
    j       context_restore
exception:
    mv      s0, a0                  /* preserve the ptr to the syscall frame */
    jal     ac_port_trap_handler
    beq     a0, s0, context_restore
    addi    sp, s0, FULL_FRAME_SZ   /* skip the syscall frame on async call */

context_restore:                    /* a0 points to context, MIE = 0 */
    lw      t6, 17*4(a0)            /* load volatile registers except a0/t0 */
    lw      t5, 16*4(a0)
    lw      t4, 15*4(a0)
    lw      t3, 14*4(a0)
    lw      a7, 13*4(a0)
    lw      a6, 12*4(a0)
    lw      a5, 11*4(a0)
    lw      a4, 10*4(a0)
    lw      a3, 9*4(a0)
    lw      a2, 8*4(a0)
    lw      a1, 7*4(a0)
    lw      t2, 5*4(a0)
    lw      t1, 4*4(a0)
    lw      ra, 2*4(a0)
    lw      t0, 1*4(a0)
    csrw    mepc, t0
    lw      t0, 0*4(a0)
    csrw    mstatus, t0
    srli    t0, t0, 11              /* MPP offset */
    andi    t0, t0, 3               /* MPP mask */
    beqz    t0, to_umode
    mv      sp, a0                  /* load a0/t0 using sp */
    lw      t0, 3*4(sp)
    lw      a0, 6*4(sp)
    addi    sp, sp, SMALL_FRAME_SZ
    mret
to_umode:
    csrw    mscratch, sp            /* save top of kernel stack to mscratch */
    mv      sp, a0
    la      a0, kregs               /* preserve kernel regs using x30-31 */
    sw      gp,  0*4(a0)
    sw      tp,  1*4(a0)
    sw      s0,  2*4(a0)
    sw      s1,  3*4(a0)
    sw      s2,  4*4(a0)
    sw      s3,  5*4(a0)
    sw      s4,  6*4(a0)
    sw      s5,  7*4(a0)
    sw      s6,  8*4(a0)
    sw      s7,  9*4(a0)
    sw      s8,  10*4(a0)
    sw      s9,  11*4(a0)
    sw      s10, 12*4(a0)
    sw      s11, 13*4(a0)
    lw      t0,  3*4(sp)            /* load scratch registers a0/t0 */
    lw      a0,  6*4(sp)
    lw      s11, 32*4(sp)           /* load user's persistent regs */
    lw      s10, 31*4(sp)
    lw      s9,  30*4(sp)
    lw      s8,  29*4(sp)
    lw      s7,  28*4(sp)
    lw      s6,  27*4(sp)
    lw      s5,  26*4(sp)
    lw      s4,  25*4(sp)
    lw      s3,  24*4(sp)
    lw      s2,  23*4(sp)
    lw      s1,  22*4(sp)
    lw      s0,  21*4(sp)
    lw      tp,  20*4(sp)
    lw      gp,  19*4(sp)           
    lw      sp,  18*4(sp)           /* SP is updated using SP, last op */
    mret

ac_kernel_start:
    csrc    mstatus, MSTATUS_MIE
    csrs    mie, MIE_MSIE           /* enable machine sw interrupts */
    li      t0, MIE_MTIE
    csrs    mie, t0                 /* enable machine timer interrupts */
    la      sp, _estack
    addi    sp, sp, -IDLE_STACK_SIZE
    csrs    mstatus, MSTATUS_MIE
wait:    
    wfi
    j       wait

