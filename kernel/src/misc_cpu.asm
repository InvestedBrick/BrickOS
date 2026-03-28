.text
global check_SSE
check_SSE:
    mov rax, 0x1
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    cpuid
    test rdx, 1 << 25
    jz .noSSE
    mov rax,1
    ret
.noSSE:
    xor rax,rax
    ret

global enable_SSE
enable_SSE:
    mov rax, cr0
    and ax, 0xFFFB		;clear coprocessor emulation CR0.EM
    or ax, 0x2			;set coprocessor monitoring  CR0.MP
    mov cr0, rax
    mov rax, cr4
    or ax, 3 << 9		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, rax
    ret

global check_FPU
check_FPU:
    mov rdx, cr0
    and rdx, ~(3 << 2) ;clear cr0.TS and cr0.EM
    mov cr0, rdx
    fninit
    mov ax, 0x1234
    fnstsw ax
    cmp ax, 0
    jne .noFPU
    mov rax, 1
    ret
.noFPU:
    xor rax,rax
    ret

.data
    value_37A dw 0x37a
.text
global setup_FPU
setup_FPU:
    fldcw [value_37A]
    mov rdx, cr0
    or dx, (1 << 5) ; enable cr0.NE => native fpu errors
    mov cr0, rdx
    ret
global write_msr
write_msr:
    ; rdi = MSR, rsi = low, rdx = high
    mov ecx, edi
    mov eax, esi
    wrmsr
    ret

global read_msr
read_msr:
    ; rdi = MSR, rsi = &low, rdx = &high
    mov ecx, edi
    rdmsr
    mov dword [rsi], eax
    mov dword [rdx], edx
    ret