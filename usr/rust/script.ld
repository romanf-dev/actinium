/*
 * @file    script.ld
 * @brief   pre-link script for Rust tasks.
 */

ENTRY(main)

SECTIONS
{
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.note.GNU-stack)
        *(.debug*)
        *(.llvm*)
    }
}

