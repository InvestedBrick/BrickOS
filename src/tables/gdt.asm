
global load_gdt
section .data
    flush_cs_ptr: dq flush_cs
section .text

; load_gdt: loads a global descriptor table
; stack:    [esp + 4] address to a gdt struct
;           [esp    ] return address
;
load_gdt:
    mov eax, [esp + 4]
    lgdt [eax]   ;         offset idx     ti   rpl
    mov ax, 0x10 ; 0x10 = 00000000 00010 | 0 | 0 0 |
    mov ds, ax   
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp qword [rel flush_cs_ptr] ; load the code segment
flush_cs:
    ret

global load_tss
load_tss:
    ; loads the task state segment
    ; stack: [esp + 4] segment index
    ;        [esp    ] return address
    mov eax, [esp + 4]
    ltr ax
    ret