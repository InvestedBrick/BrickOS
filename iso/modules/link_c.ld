SECTIONS
{
    . = 0;
    .text ALIGN(4) : {
        KEEP(*(.text.start))
        *(.text)
    } :text

    .data ALIGN(4) : {
        *(.data)
    } :data

    .rodata ALIGN(4) : {
        *(.rodata*)
    } :rodata
}

PHDRS
{
    text PT_LOAD FLAGS(0x5);    /* R_X */
    data PT_LOAD FLAGS(0x6);    /* RW_ */
    rodata PT_LOAD FLAGS(0x4);  /* R__ */
}

ENTRY(main)