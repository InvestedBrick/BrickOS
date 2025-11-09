
global load_gdt
; load_gdt: loads a global descriptor table
; first arg (rdi) address of the gdt
load_gdt:
    mov rax, rdi
    lgdt [rax]   ;         offset idx     ti   rpl
    push 0x08
    lea rax, [rel .reload_segments]
    push rax
    retfq ; far return
.reload_segments:
    mov ax, 0x10 ; 0x10 = 00000000 00010 | 0 | 0 0 | => data segment
    mov ds, ax   
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global load_tss
load_tss:
    ; loads the task state segment
    mov ax, di
    ltr ax
    ret