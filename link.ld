OUTPUT_FORMAT(elf-x86-64)
ENTRY(start)

SECTIONS {
    . = 1M;

    kernel_physical_start = .;
    kernel_virtual_start = . + 0xffffffff80000000;

    .multiboot ALIGN(8) : {
        *(.multiboot)
    }

    .note.gnu.build-id ALIGN(4K): {
        *(.note.gnu.build-id)
    }

    .boot ALIGN(4K) : {
        *(.boot)
    }

    . += 0xffffffff80000000;
    .text ALIGN(4K): AT(ADDR(.text) - 0xffffffff80000000){
        *(.text)
    }

    .rodata ALIGN(4K): AT(ADDR(.rodata) - 0xffffffff80000000){
        *(.rodata*)
    }
    .data ALIGN(4K): AT(ADDR(.data) - 0xffffffff80000000){
        *(.data*)
    }
    .bss ALIGN(4K): AT(ADDR(.bss) - 0xC0000000){
        *(COMMON)
        *(.bss*)
    }

    kernel_physical_end = . - 0xffffffff80000000;
    kernel_virtual_end = .;
}