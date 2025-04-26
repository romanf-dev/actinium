/**
 ******************************************************************************
 * @file      task.ld
 * @brief     Linker script for unprivileged tasks.
 *****************************************************************************/

ENTRY(main)

SECTIONS
{
    .text : {
        KEEP(*(.startup))
        *(.text*)   
        *(.rodata*)
        *(.srodata*)
        _etext = .;
    } 

    .data : AT (_etext) {
        _sdata = .;
        *(.data*)
        *(.sdata*)
        _edata = .;
    } 

    .bss : {
        _sbss = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        _ebss = .;
    }

    /DISCARD/ : 
    {	
        *(.riscv.attributes)
    }
}

