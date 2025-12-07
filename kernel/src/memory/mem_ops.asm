global invalidate

invalidate:
    mov rax, rdi
    invlpg [rax]
    ret

global mem_get_current_pml4_phys
mem_get_current_pml4_phys:
    mov rax, cr3
    ret

global mem_set_current_pml4_phys
mem_set_current_pml4_phys:
    mov rax, rdi
    mov cr3, rax
    ret

global flush_tlb
flush_tlb:
    mov rax, cr3
    mov cr3, rax
    ret
