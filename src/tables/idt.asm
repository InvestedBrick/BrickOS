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

    call interrupt_handler

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

global idt_code_table
idt_code_table:
%assign i 0
%rep    32
    dd interrupt_handler_%+i
%assign i i+1
%endrep