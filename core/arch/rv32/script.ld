/*
 * Linker script for RISCV HAL.
 * ldgen script assumes that kernel binary only contains text/data/bss 
 * sections while riscv ABI defines also srodata/sdata/sbss so this
 * script just merges related sections into single one.
 */

SECTIONS
{
    .text : {
        *(.text*)   
        *(.rodata*)
        *(.srodata*)
    } 

    .data : {
        *(.data*)
        *(.sdata*)
    } 

    .bss : {
        *(.bss*)
        *(.sbss*)
        *(COMMON)
    }

    /DISCARD/ : 
    {	
        *(.riscv.attributes)
    }
}

