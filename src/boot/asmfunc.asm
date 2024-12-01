global gdt_flush,tss_flush,idt_flush,switch_to,copy_page_physical
global asm_syscall_handler

extern syscall

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

tss_flush:
    mov ax, 0x2B
    ltr ax
    ret

idt_flush: ; void idt_flush(uint32_t);
    mov eax, [esp + 4]
    lidt [eax]
    ret

thread_switch_to: ;void thread_switch_to(struct thread_context *prev, struct thread_context *next);


switch_to: ;void switch_to(struct context *prev, struct context *next);
        mov eax, [esp+4]

        mov [eax+0],  esp
        mov [eax+4],  ebp
        mov [eax+8],  ebx
        mov [eax+12], esi
        mov [eax+16], edi
        mov [eax+20], ecx
        mov [eax+24], edx

        pushf        ;保存eflags
        pop ecx
        mov [eax+28], ecx

        mov eax, [esp+8]

        mov esp, [eax+0]
        mov ebp, [eax+4]
        mov ebx, [eax+8]
        mov esi, [eax+12]
        mov edi, [eax+16]
        mov ecx, [eax+20]
        mov edx, [eax+24]

        mov eax, [eax+28] ;加载eflags
        push eax
        popf

        ret
copy_page_physical:
    push ebx              ; According to __cdecl, we must preserve the contents of EBX.
    pushf                 ; push EFLAGS, so we can pop it and reenable interrupts
                          ; later, if they were enabled anyway.
    cli                   ; Disable interrupts, so we aren't interrupted.
                          ; Load these in BEFORE we disable paging!
    mov ebx, [esp+12]     ; Source address
    mov ecx, [esp+16]     ; Destination address

    mov edx, cr0          ; Get the control register...
    and edx, 0x7fffffff   ; and...
    mov cr0, edx          ; Disable paging.

    mov edx, 1024         ; 1024*4bytes = 4096 bytes

.loop:
    mov eax, [ebx]        ; Get the word at the source address
    mov [ecx], eax        ; Store it at the dest address
    add ebx, 4            ; Source address += sizeof(word)
    add ecx, 4            ; Dest address += sizeof(word)
    dec edx               ; One less word to do
    jnz .loop

    mov edx, cr0          ; Get the control register again
    or  edx, 0x80000000   ; and...
    mov cr0, edx          ; Enable paging.

    popf                  ; Pop EFLAGS back.
    pop ebx               ; Get the original value of EBX back.
    ret

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