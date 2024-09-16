.code32

.set MAGIC, 0x1badb002
.set FLAGS, 7
/*.set FLAGS, 3 */
.set CHECKSUM, -(MAGIC + FLAGS)
.set MODE_TYPE, 0
.set WIDTH, 1280  /* requested width */
.set HEIGHT, 768  /* requested height */
/* .set WIDTH, 640
.set HEIGHT, 480 */
.set DEPTH, 32    /* requested bits per pixel BPP */

.set HEADER_ADDR, 0
.set LOAD_ADDR, 0
.set LOAD_END_ADDR, 0
.set BSS_END_ADDR, 0
.set ENTRY_ADDR, 0

.section .multiboot
.long MAGIC
    .long FLAGS
    .long CHECKSUM
    .long HEADER_ADDR
    .long LOAD_ADDR
    .long LOAD_END_ADDR
    .long BSS_END_ADDR
    .long ENTRY_ADDR
    .long MODE_TYPE
    .long WIDTH
    .long HEIGHT
    .long DEPTH
    /* enough space for the returned header */
    .space 4 * 13

.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

.section .text
.global _start
.type _start, @function
_start:
	mov $stack_top, %esp

    push %ebx
	call kernel_main

	cli
1:	hlt
	jmp 1b

.size _start, . - _start