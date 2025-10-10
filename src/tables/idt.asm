global load_idt
; load_idt - Loads the interrupt descriptor table (IDT).
load_idt:
    mov  rax, rdi
    lidt [rax]
    ret

insert_mode_regs:
    push rax
    ; stack state:
    ; [rsp + 40] rflags
    ; [rsp + 32] cs
    ; [rsp + 24] rip
    ; [rsp + 16] error code
    ; [rsp + 8 ] return address
    ; [rsp     ] rax

    mov rax, qword [rsp + 32] ; cs
    and rax, 0x3
    cmp rax, 0 ; check for priv level 0
    jne .return ; if priv level not equal to kernel priv -> ss and esp are already pushed

    sub rsp, 16 ; create the data for two dummy values, will be discarded anyways, just used to align data

    ; realign important values
    ; rax
    mov rax, qword [rsp + 16]
    mov qword [rsp], rax

    ; return address
    mov rax, qword [rsp + 24]
    mov qword [rsp + 8], rax
.return:
    pop rax
    ret

clear_IOPL:
    push rax
    ; Clear IOPL in EFLAGS (set to 0)
    pushfq
    pop rax
    and rax, 0xFFFFFFFFFFFFCFFF   ; Clear bits 12 and 13 (IOPL)
    push rax
    popfq
    pop rax
    ret


%macro no_error_code_interrupt_handler 1
interrupt_handler_%+%1:
    call clear_IOPL
    push qword 0    ; push 0 as error code
    call insert_mode_regs
    push qword %1    ; push the interrupt number
    jmp common_interrupt_handler
%endmacro

%macro error_code_interrupt_handler 1
interrupt_handler_%+%1:
break_%+%1:
    call clear_IOPL
    call insert_mode_regs
    push qword %1        ; push interrupt number
    jmp common_interrupt_handler
%endmacro
extern interrupt_handler

common_interrupt_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp

    push fs
    push gs

    ; set up kernel data segments
    mov ax, 0x10
    mov fs, ax
    mov gs, ax

    mov rdi, rsp
   
    sub rsp, 8 ; align stack to 16 for C calling convention
    call interrupt_handler
    add rsp, 8

    pop gs
    pop fs

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