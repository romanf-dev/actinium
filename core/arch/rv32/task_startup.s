/** 
  ******************************************************************************
  *  @file   task_startup.s
  *  @brief  Low-level syscall and startup code for Actinium usermode tasks.
  *****************************************************************************/

.section .startup

startup:
    mv      t0, ra
    bne     t0, zero, task_run
    la      t0, _sbss
    la      t1, _ebss
bss_loop:
    beq     t0, t1, data_loop
    sb      zero, 0(t0)
    addi    t0, t0, 1
    j       bss_loop
data_loop:
    la      t0, _etext
    la      t1, _sdata
    la      t2, _edata
data_init:
    beq     t1, t2, init_done
    lb      t3, (t0)
    sb      t3, (t1)
    addi    t0, t0, 1
    addi    t1, t1, 1
    j       data_init
init_done:
    mv      a0, zero
task_run:
    jal     main    
    ecall   /* input value is in a0 returned by the main */

.section .text

.global _ac_syscall
_ac_syscall:
    ecall
    ret

.weak _ac_init_once
_ac_init_once:
    ret

