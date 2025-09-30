OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

SECTIONS {
    . = 0x100000;

    kernel_physical_start = .;
    kernel_virtual_start = . + 0xc0000000;

    .multiboot ALIGN(4) : {
        *(.multiboot)
    }

    .note.gnu.build-id ALIGN(4K): {
        *(.note.gnu.build-id)
    }

    .boot ALIGN(4K) : {
        *(.boot)
    }

    . += 0xc0000000;
    .text ALIGN(4K): AT(ADDR(.text) - 0xC0000000){
        *(.text)
    }

    .rodata ALIGN(4K): AT(ADDR(.rodata) - 0xC0000000){
        *(.rodata)
        *(.rodata.*)
    }
    .data ALIGN(4K): AT(ADDR(.data) - 0xC0000000){
        *(.data)
        *(.data.*)
    }
    .bss ALIGN(4K): AT(ADDR(.bss) - 0xC0000000){
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    kernel_physical_end = . - 0xc0000000;
    kernel_virtual_end = .;
}