global put_char

put_char:
    push eax
    mov eax, 21h
    int 31h
    pop eax
    ret