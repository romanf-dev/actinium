/** 
  ******************************************************************************
  *  @file   startup.s
  *  @brief  Low level entry point.
  *****************************************************************************/

.global Reset_Handler

.set MIE_MSIE,8
.set MIE_MTIE,0x80
.set MSTATUS_MIE,8

.section .entry
.align 4

Reset_Handler:
    j       startup

NMI_Handler:
    j       .

.section .text
.align 4
startup:
	la      t0, ac_port_intr_entry
	csrw    mtvec, t0
	la      sp, _estack
	csrc    mstatus, MSTATUS_MIE

    la      t0, _bss
    la      t1, _ebss
1:
    beq     t0, t1, 2f
    sb      zero, (t0)
    addi    t0, t0, 1
    j       1b
2:
	j       main

.global mie_init
mie_init:
	csrc    mstatus, MSTATUS_MIE
    csrs    mie, MIE_MSIE           /* enable machine sw interrupts */
    li      t0, MIE_MTIE
    csrs    mie, t0                 /* enable machine timer interrupts */
    ret

