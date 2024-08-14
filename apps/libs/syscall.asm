global put_char
global debug_test

put_char:
    ret
    push	edx
    push	eax
    mov edx,[ss:esp+12]
    mov eax,0x01
    int 31h
    pop	eax
    pop	edx
    ret

debug_test:
    push edx
    mov edx,0x02
    int 31h
    pop edx
    ret