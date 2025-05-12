/** 
 *  @file   kernel.ld
 *  @brief  Kernel linker script for SiFive e31.
 */

OUTPUT_ARCH( "riscv" )

MEMORY
{
    xip (rx) : ORIGIN = 0x20400000, LENGTH = 64k
	sram (rw) : ORIGIN = 0x80000000, LENGTH = 16k
}

_estack = ORIGIN(sram) + LENGTH(sram);

SECTIONS
{
	.text : 
	{
		. = ALIGN(0x10);
		*(.entry);
		. = ALIGN(0x10);
		*(.text*);
		*(.rodata*);
		*(.srodata*);
        _etext = .;
	} > xip

    .data : AT(_etext)
    {
		_data = .;
		*(.data*);
		*(.sdata*);
		_edata = .;        
    } > sram

	.bss :
	{
		_bss = .;
		*(.bss*);
		*(.sbss*);
		_ebss = .;
	} > sram

    /DISCARD/ : 
    {	
        *(.riscv.attributes)
    }

	_bss_sz   = _ebss - _bss;
}

