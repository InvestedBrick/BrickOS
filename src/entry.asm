bits 32

mboot_page_align equ 0x1
mboot_mem_info equ 0x2
mboot_use_gfx equ 0

mboot_magic equ 0x1badb002
mboot_flags equ mboot_page_align | mboot_mem_info | mboot_use_gfx
mboot_checksum equ -(mboot_magic + mboot_flags) 

section .multiboot
align 4
    dd mboot_magic
    dd mboot_flags
    dd mboot_checksum
    dd 0, 0, 0, 0, 0

    dd 0 ; linear graphic mode
    dd 800 ;width of screen
    dd 600 ; height of screen
    dd 32 ;depth

section .bss

global stack_top

align 16
stack_bottom:
    resb 16384 * 8
stack_top:

extern kmain ; external C function 
section .boot
global start
start:
    mov ecx, (initial_page_dir - 0xc0000000)
    mov cr3, ecx 

    mov ecx, cr4 ; read current cr4
    or ecx, 0x10 ; Set Page size extentions => 4 mb pages
    mov cr4, ecx

    mov ecx, cr0 
    or ecx, 0x80000000 ;set page enable
    mov cr0, ecx

    jmp higher_half

extern kernel_physical_start
extern kernel_physical_end 
extern kernel_virtual_start 
extern kernel_virtual_end
section .text
higher_half:
    mov esp, stack_top
    push ebx ; multiboot info
    xor ebp, ebp
    call kmain

halt:
    hlt
    jmp halt


section .data
align 4096
global initial_page_dir
initial_page_dir:
    dd 10000011b ; present / rw / page size 4mb => not page table
    times 768-1 dd 0
    ; remap to higher-half kernel
    dd (0 << 22) | 10000011b 
    dd (1 << 22) | 10000011b 
    dd (2 << 22) | 10000011b 
    dd (3 << 22) | 10000011b 
    times 256-4 dd 0
