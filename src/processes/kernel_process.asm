global kernel_process_init

kernel_process_init:
    push eax
    mov eax, 1
    int 0x30
    pop eax
    ret