section .text

global memcpy
; rdi = dest, rsi = src, rdx = n
memcpy:
    test rdx, rdx
    je .done

    mov rax, rdi 
    mov rcx, rdx
    rep movsb ; move from rsi to rdi ; rcx times
    ret

.done:
    mov rax, rdi
    ret

; rdi = dest, rsi = val, rdx = n
global memset
memset:
    test rdx, rdx
    je .done

    mov rcx,rdx
    mov rax,rsi
    rep stosb ; store al into rdi, rcs times
.done:
    mov rax, rdi
    ret

; rdi = str
global strlen
strlen:
    xor rax, rax
    cmp byte [rdi], 0
    je .done
.loop:
    inc rax
    cmp byte [rdi + rax],0
    jne .loop
    ret
.done:
    ret 
    
; rdi = str, rsi = char
global find_char
find_char:
    movzx edx, byte [rdi]
    test dl, dl 
    je .not_found
    xor rax, rax
    jmp .entry
.loop:
    inc rax
    movzx edx, byte [rdi + rax]
    test dl, dl 
    je .not_found ;string terminated, was not found
.entry:
    cmp sil, dl
    jne .loop

.not_found:
    mov eax, -1
    ret