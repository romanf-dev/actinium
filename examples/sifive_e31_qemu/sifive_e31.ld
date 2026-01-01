/*
 * Linker script for the whole image (kernel + application tasks).
 * External memory.ld file contains task number and RO/RW section sizes.
 */

INCLUDE memory.ld
ASSERT(TASK_NUM == 2, "Linker script expects two tasks.")

/* Base addr + size for flash and SRAM regions for each task + task number. */
HDR_SZ = TASK_NUM * 16 + 4;

/* Kernel starts from offset 0 so just align sizes. */
KER_RO_LEN = ALIGN(KERN_RO_SZ + HDR_SZ, 32);
KER_RW_LEN = ALIGN(KERN_RW_SZ, 32);

/* Task 0 starts just after the kernel and is aligned to task's size. */
T0_RO_OFFSET = ALIGN(KER_RO_LEN, TASK0_RO_SZ);
T0_RW_OFFSET = ALIGN(KER_RW_LEN, TASK0_RW_SZ);

/* Task 1 starts after the previous task and is aligned to task's size. */
T1_RO_OFFSET = ALIGN(T0_RO_OFFSET + TASK0_RO_SZ, TASK1_RO_SZ);
T1_RW_OFFSET = ALIGN(T0_RW_OFFSET + TASK0_RW_SZ, TASK1_RW_SZ);

FLASH_BASE = 0x20400000; /* 64k */
SRAM_BASE = 0x80000000; /* 16k */

MEMORY {
	KFLASH (rx) : ORIGIN = FLASH_BASE, LENGTH = KER_RO_LEN
	KRAM (xrx)  : ORIGIN = SRAM_BASE, LENGTH = KER_RW_LEN
	FLASH0 (rx) : ORIGIN = FLASH_BASE + T0_RO_OFFSET, LENGTH = TASK0_RO_SZ
	RAM0 (rwx)  : ORIGIN = SRAM_BASE + T0_RW_OFFSET, LENGTH = TASK0_RW_SZ
	FLASH1 (rx) : ORIGIN = FLASH_BASE + T1_RO_OFFSET, LENGTH = TASK1_RO_SZ
	RAM1 (rwx)  : ORIGIN = SRAM_BASE + T1_RW_OFFSET, LENGTH = TASK1_RW_SZ
}

ENTRY(Reset_Handler)

_estack = SRAM_BASE + 0x4000;

SECTIONS {
	.text : {
		*.rel(.text*)
	} > KFLASH

	.data : {
		_data = .;
		*.rel(.data*)
		_edata = .; 
	} > KRAM AT> KFLASH

	.bss : {
        _bss = .;
		*.rel(.bss*)
		_ebss = .;
	} > KRAM

	_bss_sz   = _ebss - _bss;

	.text0 : {
	    *.task0(.text*)
	} > FLASH0

	.data0 : {
	    *.task0(.data*)
	} > RAM0 AT> FLASH0

	.bss0 : {
	    *.task0(.bss*)
	} > RAM0

	.text1 : {
	    *.task1(.text*)
	} > FLASH1

	.data1 : {
	    *.task1(.data*)
	} > RAM1 AT> FLASH1

	.bss1 : {
	    *.task1(.bss*)
	} > RAM1

	.descr (READONLY) : {
	    _ac_task_mem_map = ABSOLUTE(.);
	    LONG(TASK_NUM);
	    LONG(ADDR(.text0));
	    LONG(TASK0_RO_SZ);
	    LONG(ADDR(.data0));
	    LONG(TASK0_RW_SZ);
	    LONG(ADDR(.text1));
	    LONG(TASK1_RO_SZ);
	    LONG(ADDR(.data1));
	    LONG(TASK1_RW_SZ);
	} > KFLASH
}

