section .data

global	io_hlt, io_cli, io_sti, io_stihlt
global  io_in8,  io_in16,  io_in32
global	io_out8, io_out16, io_out32
global	io_load_eflags, io_store_eflags
global	load_gdtr, load_idtr
global tss_flush, gdt_flush, switch_to, copy_page_physical

section .text

[GLOBAL copy_page_physical]
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

[global inw]

inw:
    xor eax, eax
    push dx

    mov dx, [esp + 4]
    in ax, dx
    pop dx
    ret

[global read_port]

read_port:
	mov edx, [esp + 4]
	in al, dx
	ret

[global idt_flush]

idt_flush: ; void idt_flush(uint32_t);
    mov eax, [esp + 4]
    lidt [eax]
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

tss_flush:
    mov ax, 0x2B
    ltr ax
    ret

io_hlt:	; void io_hlt(void);
		HLT
		RET

io_cli:	; void io_cli(void);
		CLI
		RET
io_sti:	; void io_sti(void);
		STI
		RET
io_stihlt:	; void io_stihlt(void);
		STI
		HLT
		RET

io_in8:	; int io_in8(int port);
		MOV		EDX,[ESP+4]		; port
		MOV		EAX,0
		IN		AL,DX
		RET
io_in16:	; int io_in16(int port);
		MOV		EDX,[ESP+4]		; port
		MOV		EAX,0
		IN		AX,DX
		RET

io_in32:	; int io_in32(int port);
		MOV		EDX,[ESP+4]		; port
		IN		EAX,DX
		RET

io_out8:	; void io_out8(int port, int util);
		MOV		EDX,[ESP+4]		; port
		MOV		AL,[ESP+8]		; util
		OUT		DX,AL
		RET

io_out16:	; void io_out16(int port, int util);
		MOV		EDX,[ESP+4]		; port
		MOV		EAX,[ESP+8]		; util
		OUT		DX,AX
		RET

io_out32:	; void io_out32(int port, int util);
		MOV		EDX,[ESP+4]		; port
		MOV		EAX,[ESP+8]		; util
		OUT		DX,EAX
		RET

io_load_eflags:	; int io_load_eflags(void);
		PUSHFD		; PUSH EFLAGS
		POP		EAX
		RET

io_store_eflags:	; void io_store_eflags(int eflags);
		MOV		EAX,[ESP+4]
		PUSH	EAX
		POPFD		; POP EFLAGS
		RET

load_gdtr:		; void load_gdtr(int limit, int addr);
		MOV		AX,[ESP+4]		; limit
		MOV		[ESP+6],AX
		LGDT	[ESP+6]
		RET

load_idtr:		; void load_idtr(int limit, int addr);
		MOV		AX,[ESP+4]		; limit
		MOV		[ESP+6],AX
		LIDT	[ESP+6]
		RET