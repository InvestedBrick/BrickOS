global load_idt
; load_idt - Loads the interrupt descriptor table (IDT).
; stack: [esp + 4] the address of the first entry in the IDT
;        [esp    ] the return address
load_idt:
    mov  eax, [esp + 4];
    lidt [eax]
    sti  ; enable interrupts
    ret


; inserts esp and ss into the stack so that the alignment works with the interrupt struct
insert_mode_regs:
    push eax
    ; stack state:
    ; [esp + 20] eflags
    ; [esp + 16] cs
    ; [esp + 12] eip
    ; [esp + 8] error code
    ; [esp + 4] return address
    ; [esp    ] eax

    mov eax, dword [esp + 16] ; cs
    and eax, 0x3
    cmp eax, 0 ; check for priv level 0
    jne .return ; if priv level not equal to kernel priv -> ss and esp are already pushed

    sub esp, 8

    ; eax
    mov eax, dword [esp + 8]
    mov dword [esp], eax

    ; return address
    mov eax, dword [esp + 12]
    mov dword [esp + 4], eax

    ; error code
    mov eax, dword [esp + 16]
    mov dword [esp + 8], eax

    ; eip
    mov eax, dword [esp + 20]
    mov dword [esp + 12], eax

    ; cs
    mov eax, dword [esp + 24]
    mov dword [esp + 16], eax

    ;eflags
    mov eax, dword [esp + 28]
    mov dword [esp + 20], eax

    ; could I have used a loop for this => yes!
    ; did I want to deal with another variable => no!
    mov eax, esp
    add eax, 32
    mov dword [esp + 24], eax ; make esp point to the last data before the interrupt data, so that we can return to there
    xor eax,eax
    mov ax, ss
    mov dword [esp + 28], eax
.return:
    pop eax
    ret

clear_IOPL:
    push eax
    ; Clear IOPL in EFLAGS (set to 0)
    pushfd
    pop eax
    and eax, 0xcfff   ; Clear bits 12 and 13 (IOPL)
    push eax
    popfd
    pop eax
    ret


%macro no_error_code_interrupt_handler 1
interrupt_handler_%+%1:
    call clear_IOPL
    push dword 0    ; push 0 as error code
    call insert_mode_regs
    push dword %1    ; push the interrupt number
    jmp common_interrupt_handler
%endmacro

%macro error_code_interrupt_handler 1
interrupt_handler_%+%1:
break_%+%1:
    call clear_IOPL
    call insert_mode_regs
    push dword %1        ; push interrupt number
    jmp common_interrupt_handler
%endmacro
extern interrupt_handler


adjust_smaller_stack:
    push eax
    ; [esp + 24] ss
    ; [esp + 20] esp
    ; [esp + 16] eflags
    ; [esp + 12] cs
    ; [esp + 8] eip
    ; [esp + 4] ret val
    ; [esp    ] eax
    ;eflags
    mov eax, dword [esp + 16]
    mov dword [esp + 24], eax
    
    ;cs 
    mov eax, dword [esp + 12]
    mov dword [esp + 20], eax

    ; eip
    mov eax, dword [esp + 8]
    mov dword [esp + 16], eax
    
    ; return address
    mov eax, dword [esp + 4]
    mov dword [esp + 12], eax

    pop eax

    add esp, 8 ; set the stack pointer to the return address

    ret


common_interrupt_handler:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp

    push ds
    push es
    push fs
    push gs

    ; set up kernel data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax


    ; Stack
    ;   [esp + idek]        ss 
    ;   [esp + idek]        esp
    ;   [esp + 0x2c + 0x10] eflags
    ;   [esp + 0x28 + 0x10] cs
    ;   [esp + 0x24 + 0x10] eip
    ;   [esp + 0x20 + 0x10] error code            
    ;   [esp + 0x1c + 0x10] interrupt number            
    ;   [esp + 0x18 + 0x10] eax
    ;   [esp + 0x14 + 0x10] ebx
    ;   [esp + 0x10 + 0x10] ecx
    ;   [esp + 0x0c + 0x10] edx
    ;   [esp + 0x08 + 0x10] esi
    ;   [esp + 0x04 + 0x10] edi
    ;   [esp + 0x10 + 0x10] ebp
    ;   [esp + 0x0c] ds
    ;   [esp + 0x08] es
    ;   [esp + 0x04] fs
    ;   [esp       ] gs

    push esp
    call interrupt_handler
    add esp, 4
    
    pop gs
    pop fs
    pop es
    pop ds

    pop ebp
    pop edi
    pop esi 
    pop edx
    pop ecx
    pop ebx
    pop eax

    add esp, 4 ; interrupt number

    ; test the error code for the magic values we defined in scheduler.h
    cmp dword [esp], 0xffffffff
    je .smaller
    add esp, 4
    jmp .return
.smaller:
    add esp, 4
    call adjust_smaller_stack ; iret does not require ss and esp since no context switch -> remove them
.return:
    sti
    iret

; with error codes are 8, 10, 11, 12, 13, 14, 17 and 30 (but we start at 0)
no_error_code_interrupt_handler 0
no_error_code_interrupt_handler 1
no_error_code_interrupt_handler 2
no_error_code_interrupt_handler 3
no_error_code_interrupt_handler 4
no_error_code_interrupt_handler 5
no_error_code_interrupt_handler 6
no_error_code_interrupt_handler 7
error_code_interrupt_handler 8
no_error_code_interrupt_handler 9
error_code_interrupt_handler 10
error_code_interrupt_handler 11
error_code_interrupt_handler 12
error_code_interrupt_handler 13
error_code_interrupt_handler 14
no_error_code_interrupt_handler 15
no_error_code_interrupt_handler 16
error_code_interrupt_handler 17
no_error_code_interrupt_handler 18
no_error_code_interrupt_handler 19
no_error_code_interrupt_handler 20
no_error_code_interrupt_handler 21
no_error_code_interrupt_handler 22
no_error_code_interrupt_handler 23
no_error_code_interrupt_handler 24
no_error_code_interrupt_handler 25
no_error_code_interrupt_handler 26
no_error_code_interrupt_handler 27
no_error_code_interrupt_handler 28
no_error_code_interrupt_handler 29
error_code_interrupt_handler 30
no_error_code_interrupt_handler 31
no_error_code_interrupt_handler 32
no_error_code_interrupt_handler 33
no_error_code_interrupt_handler 34
no_error_code_interrupt_handler 35
no_error_code_interrupt_handler 36
no_error_code_interrupt_handler 37
no_error_code_interrupt_handler 38
no_error_code_interrupt_handler 39
no_error_code_interrupt_handler 40
no_error_code_interrupt_handler 41
no_error_code_interrupt_handler 42
no_error_code_interrupt_handler 43
no_error_code_interrupt_handler 44
no_error_code_interrupt_handler 45
no_error_code_interrupt_handler 46
no_error_code_interrupt_handler 47
no_error_code_interrupt_handler 48

global idt_code_table
idt_code_table:
%assign i 0
%rep    49
    dd interrupt_handler_%+i
%assign i i+1
%endrep