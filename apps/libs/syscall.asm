global put_char
global debug_test

put_char:
    push	eax
    mov eax,0x01
    int 31h
    pop	eax
    ret

debug_test:
    push edx
    mov edx,0x02
    int 31h
    pop edx
    ret