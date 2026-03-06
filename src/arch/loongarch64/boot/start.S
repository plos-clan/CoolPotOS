#include "metadata.h"

#define KERNEL_VIRT_OFFSET 0xfffffffef0000000
#define EARLY_PT_PAGES      4
#define EARLY_PTE_FLAGS     0x153
#define EARLY_PTE_COUNT     (512 * EARLY_PT_PAGES)

#define CSR_CRMD      0
#define CSR_PGDL      25
#define CSR_PGDH      26
#define CSR_PGD       27
#define CSR_PWCL      28
#define CSR_PWCH      29
#define CSR_TLBRENTRY 136
#define CSR_EENTRY    12

#define CRMD_PG  0x10
#define PWCL_VAL 0x13e4d52c
#define PWCH_VAL 0x267

.section .boot, "ax"

.global _start

_start:
    pcalau12i $sp, %pc_hi20(boot_stack_top)
    addi.d    $sp, $sp, %pc_lo12(boot_stack_top)

    bl        setup_early_page_table

    li.d      $t0, KERNEL_VIRT_OFFSET
    add.d     $sp, $sp, $t0

    pcalau12i $t0, %pc_hi20(to_boot_c_start)
    ld.d      $t0, $t0, %pc_lo12(to_boot_c_start)
    jirl      $zero, $t0, 0

halt_:
    idle 0
    b halt_

.align 12
tlb_refill_entry:
    pcalau12i $t0, %pc_hi20(boot_page_table_root)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_root)
    lddir     $t0, $t0, 3
    lddir     $t0, $t0, 2
    lddir     $t0, $t0, 1
    ldpte     $t0, 0
    ldpte     $t0, 1
    tlbfill
    ertn

setup_early_page_table:
    addi.d    $sp, $sp, -16
    st.d      $ra, $sp, 8

    pcalau12i $a0, %pc_hi20(boot_page_table_root)
    addi.d    $a0, $a0, %pc_lo12(boot_page_table_root)
    li.d      $a1, 4096 * 13
    bl        _memzero

    pcalau12i $t0, %pc_hi20(boot_page_table_root)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_root)
    pcalau12i $t1, %pc_hi20(boot_page_table_l2_low)
    addi.d    $t1, $t1, %pc_lo12(boot_page_table_l2_low)
    st.d      $t1, $t0, 0

    pcalau12i $t1, %pc_hi20(boot_page_table_l2_high)
    addi.d    $t1, $t1, %pc_lo12(boot_page_table_l2_high)
    li.d      $t2, 511 * 8
    add.d     $t3, $t0, $t2
    st.d      $t1, $t3, 0

    pcalau12i $t0, %pc_hi20(boot_page_table_l2_low)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_l2_low)
    pcalau12i $t1, %pc_hi20(boot_page_table_l1_low)
    addi.d    $t1, $t1, %pc_lo12(boot_page_table_l1_low)
    li.d      $t2, 2 * 8
    add.d     $t3, $t0, $t2
    st.d      $t1, $t3, 0

    pcalau12i $t0, %pc_hi20(boot_page_table_l2_high)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_l2_high)
    pcalau12i $t1, %pc_hi20(boot_page_table_l1_high)
    addi.d    $t1, $t1, %pc_lo12(boot_page_table_l1_high)
    li.d      $t2, 510 * 8
    add.d     $t3, $t0, $t2
    st.d      $t1, $t3, 0

    pcalau12i $t0, %pc_hi20(boot_page_table_l1_low)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_l1_low)
    li.d      $t1, 128 * 8
    add.d     $t0, $t0, $t1
    pcalau12i $t1, %pc_hi20(boot_page_table_pt_low)
    addi.d    $t1, $t1, %pc_lo12(boot_page_table_pt_low)
    li.d      $t2, EARLY_PT_PAGES
.Llink_low_l1:
    st.d      $t1, $t0, 0
    addi.d    $t0, $t0, 8
    li.d      $t3, 4096
    add.d     $t1, $t1, $t3
    addi.d    $t2, $t2, -1
    bnez      $t2, .Llink_low_l1

    pcalau12i $t0, %pc_hi20(boot_page_table_l1_high)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_l1_high)
    pcalau12i $t1, %pc_hi20(boot_page_table_pt_high)
    addi.d    $t1, $t1, %pc_lo12(boot_page_table_pt_high)
    li.d      $t2, EARLY_PT_PAGES
.Llink_high_l1:
    st.d      $t1, $t0, 0
    addi.d    $t0, $t0, 8
    li.d      $t3, 4096
    add.d     $t1, $t1, $t3
    addi.d    $t2, $t2, -1
    bnez      $t2, .Llink_high_l1

    pcalau12i $t0, %pc_hi20(boot_page_table_pt_low)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_pt_low)
    li.d      $t1, 0x90000000
    li.d      $t2, EARLY_PTE_COUNT
    li.d      $t3, EARLY_PTE_FLAGS
.Lmap_low_pt:
    or        $t4, $t1, $t3
    st.d      $t4, $t0, 0
    addi.d    $t0, $t0, 8
    li.d      $t5, 0x1000
    add.d     $t1, $t1, $t5
    addi.d    $t2, $t2, -1
    bnez      $t2, .Lmap_low_pt

    pcalau12i $t0, %pc_hi20(boot_page_table_pt_high)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_pt_high)
    li.d      $t1, 0x90000000
    li.d      $t2, EARLY_PTE_COUNT
    li.d      $t3, EARLY_PTE_FLAGS
.Lmap_high_pt:
    or        $t4, $t1, $t3
    st.d      $t4, $t0, 0
    addi.d    $t0, $t0, 8
    li.d      $t5, 0x1000
    add.d     $t1, $t1, $t5
    addi.d    $t2, $t2, -1
    bnez      $t2, .Lmap_high_pt

    li.d      $t0, PWCL_VAL
    csrwr     $t0, CSR_PWCL
    li.d      $t0, PWCH_VAL
    csrwr     $t0, CSR_PWCH

    pcalau12i $t0, %pc_hi20(boot_page_table_root)
    addi.d    $t0, $t0, %pc_lo12(boot_page_table_root)
    csrwr     $t0, CSR_PGDL
    csrwr     $t0, CSR_PGDH

    pcalau12i $t0, %pc_hi20(tlb_refill_entry)
    addi.d    $t0, $t0, %pc_lo12(tlb_refill_entry)
    csrwr     $t0, CSR_TLBRENTRY

    pcalau12i $t0, %pc_hi20(halt_)
    addi.d    $t0, $t0, %pc_lo12(halt_)
    csrwr     $t0, CSR_EENTRY

    li.d      $t0, CRMD_PG
    csrwr     $t0, CSR_CRMD

    ld.d      $ra, $sp, 8
    addi.d    $sp, $sp, 16
    ret

.align 3
to_boot_c_start:
    .dword _boot_c_start

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
boot_page_table_root:
    .skip 4096
boot_page_table_l2_low:
    .skip 4096
boot_page_table_l2_high:
    .skip 4096
boot_page_table_l1_low:
    .skip 4096
boot_page_table_l1_high:
    .skip 4096
boot_page_table_pt_low:
    .skip 4096 * EARLY_PT_PAGES
boot_page_table_pt_high:
    .skip 4096 * EARLY_PT_PAGES
