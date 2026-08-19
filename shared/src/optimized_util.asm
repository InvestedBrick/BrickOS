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
.done:
    ret 

; rdi = str1, rsi = str2
global streq
streq:
    xor rax, rax
    test rdi, rdi
    jz .done
    test rdi, rdi
    jz .done
.loop:
    mov dl, byte [rsi]
    cmp byte [rdi], dl
    jne .done
    inc rsi
    inc rdi
    test dl, dl
    jnz .loop
    mov rax, 1
.done:
    ret

; rdi = str, rsi = char
global find_char
find_char:
    xor rax, rax
.loop:
    movzx edx, byte [rdi + rax]
    test dl, dl 
    je .not_found ;string terminated, was not found
    cmp sil, dl
    je .found
    inc rax
    jmp .loop
.found:
    ret
.not_found:
    mov eax, -1
    ret