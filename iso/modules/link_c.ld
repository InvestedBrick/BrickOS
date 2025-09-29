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

    .bss ALIGN(4) : {
        *(.bss)
    } : bss
}

PHDRS
{
    text PT_LOAD FLAGS(0x5);    /* R_X */
    data PT_LOAD FLAGS(0x6);    /* RW_ */
    rodata PT_LOAD FLAGS(0x4);  /* R__ */
    bss PT_LOAD FLAGS(0x6);     /* RW_ */
}

ENTRY(main)