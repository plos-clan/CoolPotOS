.global apu_start
apu_start:
	//la	t0, 0f
	//li  t1, 0xffff800000000000
	//add
	auipc t0,0x0
	addi t0,t0,30
	csrw	stvec, t0

	ld	t0, 0(a1)	# sp
	ld	t1, 8(a1)	# satp
	ld  t2, 16(a1)  # entry
	mv	sp, t0
	csrw	satp, t1
	sfence.vma zero, zero
	
0:	la	t0, arch_ap_cpu_entry
	jalr	t2

0:	j	0b
