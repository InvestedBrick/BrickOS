global invalidate

invalidate:
    mov eax, [esp + 4]
    invlpg [eax]
    ret