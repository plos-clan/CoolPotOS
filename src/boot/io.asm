global	io_hlt, io_cli, io_sti, io_stihlt
global  io_in8,  io_in16,  io_in32
global	io_out8, io_out16, io_out32

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