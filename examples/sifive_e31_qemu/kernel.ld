/*
 * Since the kernel is relocatable this script is used for section grouping only.
 */

OUTPUT_ARCH( "riscv" )

SECTIONS {
	.text : {
		. = ALIGN(0x10);
		*(.entry);
		. = ALIGN(0x10);
		*(.text*);
		*(.rodata*);
		*(.srodata*);
	}

    .data : {
		*(.data*);
		*(.sdata*);
    }

	.bss : {
		*(.bss*);
		*(.sbss*);
        *(COMMON);
	}

    /DISCARD/ : {	
        *(.riscv.attributes)
    }
}

