bits 32

mboot_page_align equ (1 << 0)
mboot_mem_info equ   (1 << 1)
mboot_use_gfx equ    (1 << 2)

mboot_magic equ 0x1badb002
mboot_flags equ mboot_page_align | mboot_mem_info | mboot_use_gfx
mboot_checksum equ -(mboot_magic + mboot_flags) 

section .multiboot
align 8
    dd mboot_magic
    dd mboot_flags
    dd mboot_checksum

    dd 0 ; header_addr
    dd 0 ; load_addr
    dd 0 ; load_end_addr
    dd 0 ; bss_end_addr
    dd 0 ; entry_addr

    dd 0          ; mode_type: 0 = linear graphics, 1 = EGA text
    dd 1024       ; width
    dd 768        ; height
    dd 32         ; depth (bits per pixel)


global stack_top

extern kmain ; external C function 
global start
section .boot
start:
    ;Enable PAE
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ;set long mode enable
    mov ecx, 0xc0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    mov eax, initial_pml4_table
    mov cr3, eax

    mov eax, cr0
    or eax, (1 << 31) | 0x1 ; enable paging and protected mode
    mov cr0, eax
    ;TODO: Setup minimal gdt here to have a more valid long jump
    jmp 0x08:long_mode_entry

extern kernel_physical_start
extern kernel_physical_end 
extern kernel_virtual_start 
extern kernel_virtual_end

section .text
bits 64
long_mode_entry:

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, stack_top
    mov rdi, rbx ; multiboot info as first arg
    xor rbp, rbp
    call kmain

halt:
    hlt
    jmp halt

section .bss
align 16
stack_bottom:
    resb 16384 * 8
stack_top:


section .data
align 4096
global initial_pml4_table
initial_pml4_table:
    dq pdpt + 0x3
    times 511 dq 0
align 4096
pdpt:
    dq pd + 0x3
    times 511 dq 0
align 4096
pd:
    dq 0x0000000000000083      ; 1GB page, identity map low memory
    times 511 dq 0

