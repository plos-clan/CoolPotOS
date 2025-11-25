.global apu_start
apu_start:
	la	t0, 0f
	li  t1, 0xffff800000000000
	add t0, t0, t1
	csrw	stvec, t0

	ld	t0, 0(a1)	# sp
	ld	t1, 8(a1)	# satp
	mv	sp, t0
	csrw	satp, t1

0: 	sfence.vma zero, zero
    la	t0, arch_ap_cpu_entry
	jalr	t0

0:	j	0b
