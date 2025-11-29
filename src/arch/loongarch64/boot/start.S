#include "metadata.h"

.section .boot, "ax"

.global _start

_start:
    pcalau12i $sp, %pc_hi20(boot_stack_top)
    addi.d $sp, $sp, %pc_lo12(boot_stack_top)

    bl setup_early_page_table

    li.d   $t0, 0xffff800000000000
    add.d  $sp, $sp, $t0

    pcalau12i $t0, %pc_hi20(_boot_c_start)
    addi.d    $t0, $t0, %pc_lo12(_boot_c_start)
    jirl      $zero, $t0, 0

halt_:
    idle 0
    b halt_

setup_early_page_table:
    ret

/*
 * 内存清零
 * $a0: 地址
 * $a1: 长度
 */
_memzero:
    beqz    $a1, .Lmemzero_done
    add.d   $a1, $a0, $a1
.Lmemzero_loop:
    st.d    $zero, $a0, 0
    addi.d  $a0, $a0, 8
    blt     $a0, $a1, .Lmemzero_loop
.Lmemzero_done:
    ret

.section .boot.data
.align 12
boot_stack:
.skip STACK_SIZE
boot_stack_top:

.align 12

.globl boot_page_table_l3
.globl boot_page_table_l2
.globl boot_page_table_l1_kernel
.globl boot_page_table_l1_dtb

boot_page_table_l3:
    .skip 4096
boot_page_table_l2:
    .skip 4096
boot_page_table_l1_kernel:
    .skip 4096
boot_page_table_l1_dtb:
    .skip 4096