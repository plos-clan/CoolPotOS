#include "../include/memory.h"
#include "../include/console.h"
#include "../include/redpane.h"
#include "../include/kheap.h"

page_directory_t *kernel_directory = 0; // 内核用页目录
page_directory_t *current_directory = 0; // 当前页目录

uint32_t *frames;
uint32_t nframes; // 两者共同组成了一个bitset，用于管理frame

extern uint32_t placement_address;
extern void *program_break, *program_break_end;

static void set_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / 0x1000; // 页中frame地址右移了12位，此处也一样
    uint32_t idx = INDEX_FROM_BIT(frame); // 找到对应位置
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames[idx] |= (0x1 << off); // off位置1
}

static void clear_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / 0x1000; // 页中frame地址右移了12位，此处也一样
    uint32_t idx = INDEX_FROM_BIT(frame); // 找到对应位置
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames[idx] &= ~(0x1 << off); // off位置0
}

static uint32_t test_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / 0x1000; // 页中frame地址右移了12位，此处也一样
    uint32_t idx = INDEX_FROM_BIT(frame); // 找到对应位置
    uint32_t off = OFFSET_FROM_BIT(frame);
    return (frames[idx] & (0x1 << off)); // 只取出off位对应的值
    // 原理类似clear_frame，但是把off位以外的清0
}

static uint32_t first_frame() // 第一个空frame在哪里？
{
    for (int i = 0; i < INDEX_FROM_BIT(nframes); i++) {
        if (frames[i] != 0xffffffff) { // 0xffffffff，已经分完了，不必细看
            for (int j = 0; j < 32; j++) { // 至少有一位是空着的
                uint32_t toTest = 0x1 << j; // 把1推到第j位
                if (!(frames[i] & toTest)) { // 若此时检查出来为0，则找到空处，直接返回
                    return i * 4 * 8 + j; // 第i个索引第j位，转化为正常索引
                }
            }
        }
    }
    return (uint32_t) - 1; // 都没有，返回-1
}

// bitset结束，下面该运用bitset来分配和释放frame了
void alloc_frame(page_t *page, int is_kernel, int is_writable) {
    if (page->frame) return; // 传入的page已经有frame，无需分配
    else {
        uint32_t idx = first_frame();
        if (idx == (uint32_t) - 1) {
            show_VGA_red_pane("FRAMES_FREE_ERROR", "Cannot free frames!", 0x00, NULL);
        }
        set_frame(idx * 0x1000); // 标记当前frame，现在是我的了
        page->present = 1; // 现在这个页存在了
        page->rw = is_writable ? 1 : 0; // 是否可写由is_writable决定
        page->user = is_kernel ? 0 : 1; // 是否为用户态由is_kernel决定
        page->frame = idx; // 得到的frame索引，记录
    }
}

void free_frame(page_t *page) {
    uint32_t frame = page->frame;
    if (!frame) return; // 当前页没有frame，无需处理
    else {
        clear_frame(frame); // 现在这个frame不归我管了
        page->frame = 0x0; // 页也没有frame了
    }
}

// 最后的最后，正式的分页代码
void init_page() {
    uint32_t mem_end_page = 0xFFFFFFFF; // 内存最大到何处，假定为4GB

    nframes = mem_end_page / 0x1000; // frames多少
    frames = (uint32_t *) kmalloc(INDEX_FROM_BIT(nframes)); // 为bitset分配空间
    memset(frames, 0, INDEX_FROM_BIT(nframes)); // 全部置0

    // 初始化页目录
    kernel_directory = (page_directory_t *) kmalloc_a(sizeof(page_directory_t)); //kmalloc: 无分页情况自动在内核后方分配 | 有分页从内核堆分配
    // 页目录要求page-aligned，因此用kmalloc_a
    memset(kernel_directory, 0, sizeof(page_directory_t));
    current_directory = kernel_directory;
    int i = 0;
    while (i < placement_address) {
        // 内核部分对ring3而言可读不可写 | 无偏移页表映射
        alloc_frame(get_page(i, 1, kernel_directory), 0, 0);
        i += 0x1000;
    }

    for (int i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i++) {
        alloc_frame(get_page(i, 1, kernel_directory), 0, 0);
    }

    register_interrupt_handler(14, page_fault);
    switch_page_directory(kernel_directory);

    program_break = (void *) KHEAP_START;
    program_break_end = (void *) (KHEAP_START + KHEAP_INITIAL_SIZE);
}

void switch_page_directory(page_directory_t *dir) {
    current_directory = dir;
    asm volatile("mov %0, %%cr3" : : "r"(&dir->tablesPhysical));
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

page_t *get_page(uint32_t address, int make, page_directory_t *dir) {
    address /= 0x1000; // 将地址转化为bitset索引
    uint32_t table_idx = address / 1024; // 找到包含这一索引的页表
    if (dir->tables[table_idx]) return &dir->tables[table_idx]->pages[address % 1024]; // 已经被赋值了，返回现成的
    else if (make) { // 没有，但可以造页
        uint32_t tmp;
        dir->tables[table_idx] = (page_table_t *) kmalloc_ap(sizeof(page_table_t), &tmp); // 直接创造一个页表，tmp为其物理地址
        memset(dir->tables[table_idx], 0, 0x1000); // 页表清空
        dir->tablesPhysical[table_idx] = tmp | 0x7; // 存在，可读写，用户态
        return &dir->tables[table_idx]->pages[address % 1024]; // 新鲜出炉的页
    } else return 0; // 否则啥也干不了，只能返回0
}

void page_fault(registers_t *regs) {
    // 现在一个#PE错误出现，根据标准，错误地址在cr2寄存器中。
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address)); // 将cr2赋给faulting_address
    // error code中有更详细的信息
    int present = !(regs->err_code & 0x1); // 页不存在？
    int rw = regs->err_code & 0x2; // 只读页被写入？
    int us = regs->err_code & 0x4; // 用户态写入内核页？
    int reserved = regs->err_code & 0x8; // 写入CPU保留位？
    int id = regs->err_code & 0x10; // 由取指引起？

    // 红屏错误
    println("[ERROR]: Page fault");
    if (present)
        show_VGA_red_pane("PAGE_TABLE_FAULT", "CrashPowerDOS page table fault: present", faulting_address, regs);
    if (rw) show_VGA_red_pane("PAGE_TABLE_FAULT", "CrashPowerDOS page table fault: read-only", faulting_address, regs);
    if (us) show_VGA_red_pane("PAGE_TABLE_FAULT", "CrashPowerDOS page table fault: user-mode", faulting_address, regs);
    if (reserved)
        show_VGA_red_pane("PAGE_TABLE_FAULT", "CrashPowerDOS page table fault: reserved", faulting_address, regs);
}

