global enter_user_mode

USER_MODE_CODE_SEGMENT_SELECTOR equ (0x18 | 0x3)
USER_MODE_DATA_SEGMENT_SELECTOR equ (0x20 | 0x3)
USER_MODE_STACK_POINTER equ 0xbffffffb
USER_MODE_INTRUC_POINTER equ 0x00000000
USER_MODE_EFLAGS equ (1 << 9) ; enable interrupts
enter_user_mode:
    mov eax, USER_MODE_DATA_SEGMENT_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ;Stack for iret:
    ; [esp + 16] ss
    ; [esp + 12] esp
    ; [esp + 8] eflags
    ; [esp + 4] cs
    ; [esp    ] eip
    push USER_MODE_DATA_SEGMENT_SELECTOR
    push USER_MODE_STACK_POINTER
    push USER_MODE_EFLAGS
    push USER_MODE_CODE_SEGMENT_SELECTOR
    push USER_MODE_INTRUC_POINTER
    
    iret
