global put_char

put_char:
    push	edx
    push	eax
    mov edx,[ss:esp+12]
    mov eax,0x01
    int 31h
    pop	eax
    pop	edx
    ret