global invalidate

%define KERNEL_START 0xffff800000000000
invalidate:
    mov rax, rdi
    invlpg [rax]
    ret

global mem_get_current_page_dir

mem_get_current_page_dir:
    mov rax, cr3
    mov rcx, KERNEL_START
    add rax, rcx 
    ret

global mem_set_current_page_dir
mem_set_current_page_dir:
    mov rax, rdi
    mov cr3, rax
    ret
