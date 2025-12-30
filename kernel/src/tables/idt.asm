global load_idt
; load_idt - Loads the interrupt descriptor table (IDT).
load_idt:
    mov  rax, rdi
    lidt [rax]
    ret

%macro no_error_code_interrupt_handler 1
interrupt_handler_%+%1:
    push qword 0    ; push 0 as error code
    push qword %1    ; push the interrupt number
    jmp common_interrupt_handler
%endmacro

%macro error_code_interrupt_handler 1
interrupt_handler_%+%1:
break_%+%1:
    push qword %1        ; push interrupt number
    jmp common_interrupt_handler
%endmacro
extern interrupt_handler
global return_from_interrupt
common_interrupt_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp

    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    push fs
    push gs

    ; set up kernel data segments
    mov ax, 0x10
    mov fs, ax
    mov gs, ax

    mov rdi, rsp
   
    call interrupt_handler

return_from_interrupt:
    cli
    pop gs
    pop fs

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8

    pop rbp
    pop rdi
    pop rsi 
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16 ; interrupt number and error code

    iretq

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
    dq interrupt_handler_%+i
%assign i i+1
%endrep