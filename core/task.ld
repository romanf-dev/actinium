ENTRY(main)

SECTIONS
{
    .text : {
        *(.startup)
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

