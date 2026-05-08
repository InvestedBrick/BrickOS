bits 64
section .text
    mov rax, 0x10
    int 0x30

    mov rdx, 6
    pushfq              ; Push EFLAGS onto the stack
    pop rax             ; Pop into EAX

    jmp $