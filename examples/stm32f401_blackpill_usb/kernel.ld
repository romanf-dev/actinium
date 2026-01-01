/*
 * Since the kernel is relocatable, memory section is absent.
 * This script is used for section grouping only.
 */

SECTIONS {
    .text : {
        KEEP(*(.isr_vector)) /* Startup code */
        . = ALIGN(4);
        *(.text)           /* .text sections (code) */
        *(.text*)          /* .text* sections (code) */
        *(.glue_7)         /* glue arm to thumb code */
        *(.glue_7t)        /* glue thumb to arm code */
        *(.eh_frame)
        KEEP (*(.init))
        KEEP (*(.fini))
        . = ALIGN(4);
        *(.rodata)         /* .rodata sections (constants, strings, etc.) */
        *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
        . = ALIGN(4);
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
        . = ALIGN(4);
    }

    /* Initialized data sections into "RAM" Ram type memory */
    .data : {
        . = ALIGN(4);
        *(.data)           /* .data sections */
        *(.data*)          /* .data* sections */
        *(.RamFunc)        /* .RamFunc sections */
        *(.RamFunc*)       /* .RamFunc* sections */
        . = ALIGN(4);
    }

    /* Uninitialized data section into "RAM" Ram type memory */
    . = ALIGN(4);
    .bss : {
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
    }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

