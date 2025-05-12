/*
 *  @file   task.ld
 *  @brief  Linker script for unprivileged tasks.
 */

ENTRY(main)

SECTIONS
{
    .text : {
        KEEP(*(.startup))
        *(.text*)   
        *(.rodata*)
        _etext = .;
    } 

    .data : AT (_etext) {
        _sdata = .;
        *(.data*)
        _edata = .;
    } 

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    }
}

