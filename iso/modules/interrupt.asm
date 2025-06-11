
section .text
    mov eax, 1
    int 0x30

    mov eax, 0
    int 0x30

    jmp $