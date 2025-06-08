global load_idt
; load_idt - Loads the interrupt descriptor table (IDT).
; stack: [esp + 4] the address of the first entry in the IDT
;        [esp    ] the return address
load_idt:
    mov  eax, [esp + 4];
    lidt [eax]
    sti  ; enable interrupts
    ret

%macro no_error_code_interrupt_handler 1
interrupt_handler_%+%1:
    push dword 0    ; push 0 as error code
    push dword %1    ; push the interrupt number
    jmp common_interrupt_handler
%endmacro

%macro error_code_interrupt_handler 1
interrupt_handler_%+%1:
    push dword %1        ; push interrupt number
    jmp common_interrupt_handler
%endmacro
extern interrupt_handler

common_interrupt_handler:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp


    ; Stack
    ;   [esp + 0x2c] eflags
    ;   [esp + 0x28] cs
    ;   [esp + 0x24] eip
    ;   [esp + 0x20] error code            
    ;   [esp + 0x1c] interrupt number            
    ;   [esp + 0x18] eax
    ;   [esp + 0x14] ebx
    ;   [esp + 0x10] ecx
    ;   [esp + 0x0c] edx
    ;   [esp + 0x08] esi
    ;   [esp + 0x04] edi
    ;   [esp       ] ebp

    push esp
    call interrupt_handler
    add esp, 4
    
    pop ebp
    pop edi
    pop esi 
    pop edx
    pop ecx
    pop ebx
    pop eax

    add esp, 8
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
%rep    48
    dd interrupt_handler_%+i
%assign i i+1
%endrep