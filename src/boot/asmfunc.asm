global taskX_switch
global asm_syscall_handler
global panic

taskX_switch:
    mov eax, [esp+4]

    mov [eax+0],  esp
    mov [eax+4],  ebp
    mov [eax+8],  ebx
    mov [eax+12], esi
    mov [eax+16], edi
    mov [eax+20], edx
    pushf

    ret

extern syscall


asm_syscall_handler:
    cli
    push ds
    push es
    push fs
    push gs
    pusha
    call syscall
    mov dword [esp+28], eax
    popa
    pop gs
    pop fs
    pop es
    pop ds
    IRETD

panic:
    ret;JMP 0xffff0


