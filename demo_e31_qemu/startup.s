/** 
  ******************************************************************************
  *  @file   startup.s
  *  @brief  Low level entry point.
  *****************************************************************************/

.global Reset_Handler

.section .entry

Reset_Handler:
    j       startup

NMI_Handler:
    j       .

.section .text

startup:
	la      t0, ac_port_intr_entry
	csrw    mtvec, t0
	la      sp, _estack
	csrc    mstatus, 8

    la      t0, _bss
    la      t1, _ebss
1:
    beq     t0, t1, 2f
    sb      zero, 0(t0)
    addi    t0, t0, 1
    j       1b
2:
    csrc    mie, 8
	j       main

