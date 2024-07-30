global taskX_switch
global asm_syscall_handler

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

extern syscall_handler

asm_syscall_handler:
    cli
    push byte 0
    push 80h
    pusha

    mov ax, ds
    push eax ; 存储ds

    mov ax, 0x10 ; 将内核数据段赋值给各段
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call syscall_handler
    pop eax ; 恢复
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa

    add esp, 8 ; 弹出错误码和中断ID
    iret


