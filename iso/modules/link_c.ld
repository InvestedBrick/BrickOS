SECTIONS
{
    . = 0x0000000000000000;
    .text ALIGN(16) : {
        KEEP(*(.text.start))
        *(.text)
    } :text

    .data ALIGN(16) : {
        *(.data)
    } :data

    .rodata ALIGN(16) : {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    .bss ALIGN(16) : {
        *(.bss)
        *(.bss.*)
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