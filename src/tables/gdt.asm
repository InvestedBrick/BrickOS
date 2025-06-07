
global load_gdt

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
    jmp 0x08:flush_cs ; load the code segment
flush_cs:
    ret
