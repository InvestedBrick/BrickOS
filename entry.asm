bits 32
section .text
global start
    ; grub multiboot specs
    align 4
    dd 0x1badb002       ; Magic number to identify header
    dd 0x1              ; flags
    dd - ( 0x1badb002 + 0x1) ; checksum


extern kmain ; external C function 

start:
    cli    ; clear interrupts
    mov esp, stack_space
    call kmain
    
halt:
    hlt
    jmp halt

section .bss
resb 8192 ;8kb stack size
stack_space: