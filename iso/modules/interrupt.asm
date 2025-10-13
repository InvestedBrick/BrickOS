bits 64
org 0x0000000000000000
section .text
    mov rax, 0x10
    int 0x30

    mov rdx, 6
    pushfdq              ; Push EFLAGS onto the stack
    pop rax             ; Pop into EAX

    jmp $