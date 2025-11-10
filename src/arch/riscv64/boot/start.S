#include "metadata.h"

.section .boot, "ax"

.global _opensbi_start
_opensbi_start:
    // 保存参数
    mv tp, a0              // hart_id
    mv s1, a1              // dtb_ptr

    // 设置临时栈
    la sp, boot_stack_top

    // 建立早期页表（s1 中有 dtb_ptr）
    call setup_early_page_table

    // 启用分页 (SV48)
    la t0, boot_page_table_l3
    srli t0, t0, 12        // 获取PPN
    li t1, 9               // SV48 mode = 9
    slli t1, t1, 60
    or t0, t0, t1
    csrw satp, t0
    sfence.vma zero, zero

    // 现在运行在高半核，重新设置栈到高地址
    la sp, boot_stack_top
    li t0, 0xffff800000000000
    add sp, sp, t0

    // 准备调用 opensbi_c_start
    mv a0, tp              // hart_id

    // DTB 地址转换为高半核地址
    mv a1, s1              // dtb_ptr
    li t0, 0xffff800000000000
    add a1, a1, t0

    // 直接计算 opensbi_c_start 的高地址
    ld t0, to_opensbi_c_start
    jalr ra, t0, 0

    // 如果返回，死循环
.Lhalt:
    wfi
    j .Lhalt

to_opensbi_c_start:
    .quad opensbi_c_start

// 建立早期页表的函数
// 映射：
//   1. 0x80000000-0x81000000 (内核 16MB)
//   2. DTB 所在的 2MB 对齐页
setup_early_page_table:
    // s1 = dtb_ptr
    // 保存返回地址
    mv t6, ra

    // 清零所有页表
    la a0, boot_page_table_l3
    li a1, 4096 * 4        // 4 个页表页
    call memzero

    // ===== 设置 L3 页表 =====
    la t0, boot_page_table_l3
    la t1, boot_page_table_l2
    srli t1, t1, 12        // PPN
    slli t1, t1, 10        // PPN << 10
    ori t1, t1, 0x01       // PTE_V

    // L3[0] = &L2 | V (恒等映射入口)
    sd t1, 0(t0)

    // L3[256] = &L2 | V (高半核映射入口)
    li t2, 256 * 8
    add t3, t0, t2
    sd t1, 0(t3)

    // ===== 设置 L2 页表 =====
    la t0, boot_page_table_l2

    // 映射内核区域: L2[2] = &L1_kernel | V
    // 0x80000000 的 VPN[2] = 2
    la t1, boot_page_table_l1_kernel
    srli t1, t1, 12
    slli t1, t1, 10
    ori t1, t1, 0x01       // PTE_V
    li t2, 2 * 8
    add t3, t0, t2
    sd t1, 0(t3)

    // 映射 DTB 区域
    // 计算 DTB 的 VPN[2]
    mv t1, s1              // DTB 物理地址
    srli t1, t1, 30        // VPN[2] = addr >> 30
    andi t1, t1, 0x1FF

    // 检查 DTB 是否在内核同一个 1GB 区域（VPN[2] == 2）
    li t2, 2
    beq t1, t2, .Lskip_dtb_l2  // 如果在同一区域，共用 L1

    // DTB 在不同的 1GB 区域，需要单独的 L1 页表
    la t3, boot_page_table_l1_dtb
    srli t3, t3, 12
    slli t3, t3, 10
    ori t3, t3, 0x01       // PTE_V

    // L2[VPN[2]] = &L1_dtb | V
    slli t2, t1, 3         // VPN[2] * 8
    add t4, t0, t2
    sd t3, 0(t4)

.Lskip_dtb_l2:
    // ===== 设置 L1 页表（内核区域）=====
    // 映射 0x80000000-0x81000000 (16MB = 8 个 2MB 页)
    la t0, boot_page_table_l1_kernel
    li t1, 0x80000000      // 起始物理地址
    li t2, 8               // 页数量
    li t3, 0xEF            // PTE flags

.Lmap_kernel_loop:
    srli t4, t1, 12
    slli t4, t4, 10
    or t4, t4, t3
    sd t4, 0(t0)

    addi t0, t0, 8
    li t5, 0x200000        // 2MB
    add t1, t1, t5
    addi t2, t2, -1
    bnez t2, .Lmap_kernel_loop

    // ===== 映射 DTB 所在的 2MB 页 =====
    // 计算 DTB 的 2MB 对齐地址
    mv t1, s1
    li t2, 0x1FFFFF        // 2MB - 1
    not t2, t2
    and t1, t1, t2         // t1 = DTB 2MB 对齐的起始地址

    // 确定使用哪个 L1 页表
    mv t2, s1
    srli t2, t2, 30
    andi t2, t2, 0x1FF     // VPN[2]
    li t3, 2
    beq t2, t3, .Luse_kernel_l1

    // 使用 DTB 专用 L1
    la t0, boot_page_table_l1_dtb
    j .Lcalc_dtb_index

.Luse_kernel_l1:
    la t0, boot_page_table_l1_kernel

.Lcalc_dtb_index:
    // 计算 VPN[1]（在 L1 中的索引）
    mv t2, s1
    srli t2, t2, 21
    andi t2, t2, 0x1FF     // VPN[1]
    slli t2, t2, 3         // * 8
    add t0, t0, t2         // L1 基址 + 偏移

    // 设置 DTB 页的 PTE
    srli t1, t1, 12
    slli t1, t1, 10
    li t3, 0xEF            // PTE flags
    or t1, t1, t3
    sd t1, 0(t0)

    // 恢复返回地址
    mv ra, t6
    ret

// 简单的内存清零函数
// a0 = 地址, a1 = 长度
memzero:
    beqz a1, .Lmemzero_done
    add a1, a0, a1         // 结束地址
.Lmemzero_loop:
    sd zero, 0(a0)
    addi a0, a0, 8
    blt a0, a1, .Lmemzero_loop
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