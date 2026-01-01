/*
 * Linker script for the kernel.
 * By convention, kernel image's .text section must contain the whole flash
 * image including vector tables and metadata blocks. After linking the 
 * kernel remains relocatable so no segment data provided.
 */

SECTIONS {
    .text : {
        KEEP(*(.vectors))
        KEEP(*(.metadata_block))
        *(.text*)
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        . = ALIGN(4);
        *(.bss*)
        *(COMMON)
    }
}

