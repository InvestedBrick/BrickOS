org 0x00000000
bits 32
section .text
    mov eax, 2
    mov ecx, 5
    int 0x30

    mov edx, 6
    pushfd              ; Push EFLAGS onto the stack
    pop eax             ; Pop into EAX

    jmp $