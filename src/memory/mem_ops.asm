global invalidate

invalidate:
    mov eax, [esp + 4]
    invlpg [eax]
    ret

global mem_get_current_page_dir

mem_get_current_page_dir:
    mov eax, cr3
    add eax, 0xc0000000 ; add KERNEL_START
    ret

global mem_set_current_page_dir
mem_set_current_page_dir:
    mov eax, [esp + 4]
    mov cr3, eax
    ret
