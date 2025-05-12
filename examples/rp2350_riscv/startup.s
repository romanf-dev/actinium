/*
 * @file  startup.s
 * @brief Startup code for RISC-V core in RP2350.
 */

.section .entry_point
.align 4
    csrc    mstatus, 8
    la      a0, ac_port_intr_entry
    csrw    mtvec, a0
    call    gp_init
    csrr    a0, mhartid
    bnez    a0, Reset_Handler
    la      sp, _estack0
    la      t0, _sbss
    la      t1, _ebss
bss_loop:
    beq     t0, t1, data_loop
    sb      zero, 0(t0)
    addi    t0, t0, 1
    j       bss_loop
data_loop:
    la      t0, _sidata
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
    call    main

.global gp_init
gp_init:
.align 4
.option push
.option norelax
    la      gp, __global_pointer$
.option pop
    ret

.global Reset_Handler
.align 4

Reset_Handler:
    li      a0, 0x7dfc      /* Bootrom entry. */
    jr      a0

.section .metadata_block    /* See 5.9.5.2 in the datasheet. */
.align 4

.word   0xffffded3
.word   0x11010142
.word   0x000001ff
.word   0x00000000
.word   0xab123579

