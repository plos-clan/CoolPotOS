section .text

[global read_port]

read_port:
	mov edx, [esp + 4]
	in al, dx
	ret

[global gdt_flush]

gdt_flush: ; void gdt_flush(uint32_t gdtr);
    mov eax, [esp + 4] ; [esp+4]按规定是第一个参数，即gdtr
    lgdt [eax] ; 加载新gdt指针，传入的是地址

    mov ax, 0x10 ; 新数据段
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax ; 各数据段全部划给这一数据段
    jmp 0x08:.flush ; cs没法mov，只能用farjmp/farcall，这里用farjmp刷新cs
.flush:
    ret ; 刷新完毕，返回

[global idt_flush]

idt_flush:
    mov eax, [esp + 4]
    lidt [eax] ; 类似gdt_flush，但不涉及各种段的重新赋值
    ret

[global load_eflags]

load_eflags:
    pushfd ; eflags寄存器只能用pushfd/popfd操作，将eflags入栈/将栈中内容弹入eflags
    pop eax ; eax = eflags;
    ret ; return eax;

[global store_eflags]

store_eflags:
    mov eax, [esp + 4] ; 获取参数
    push eax
    popfd ; eflags = eax;
    ret

[global load_cr0]

load_cr0:
    mov eax, cr0 ; cr0只能和eax之间mov
    ret ; return cr0;

[global store_cr0]

store_cr0:
    mov eax, [esp + 4] ; 获取参数
    mov cr0, eax ; 赋值cr0
    ret

[global load_tr]

load_tr:
    ltr [esp + 4]
    ret

[global farjmp]

farjmp:
    jmp far [esp + 4]
    ret

[GLOBAL tss_flush]    ; Allows our C code to call tss_flush().
tss_flush:
    mov ax, 0x2B      ; Load the index of our TSS structure - The index is
                      ; 0x28, as it is the 5th selector and each is 8 bytes
                      ; long, but we set the bottom two bits (making 0x2B)
                      ; so that it has an RPL of 3, not zero.
    ltr ax            ; Load 0x2B into the task state register.
    ret