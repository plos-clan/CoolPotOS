#include "../include/kheap.h"
#include "../include/memory.h"

header_t *head = NULL, *tail = NULL; // 内存块链表
extern page_directory_t *kernel_directory;
extern uint32_t end; // declared in linker.ld
static uint32_t placement_address = (uint32_t) &end;
void *program_break, *program_break_end;

static uint32_t kmalloc_int(uint32_t sz, uint32_t align, uint32_t *phys) {
    if (program_break) {
        // 有内存堆
        void *addr = alloc(sz); // 直接malloc，align丢掉了
        if (phys) {
            // 需要物理地址，先找到对应页
            page_t *page = get_page((uint32_t) addr, 0, kernel_directory);
            *phys = page->frame * 0x1000 + ((uint32_t) addr & 0xfff);
        }
        return (uint32_t) addr;
    }
    if (align && (placement_address & 0x00000FFF)) {
        placement_address &= 0xFFFFF000;
        placement_address += 0x1000;
    }
    if (phys) *phys = placement_address;
    uint32_t tmp = placement_address;
    placement_address += sz;
    return tmp;
}

uint32_t kmalloc_a(uint32_t size) {
    return kmalloc_int(size, 1, 0);
}

uint32_t kmalloc_p(uint32_t size, uint32_t *phys) {
    return kmalloc_int(size, 0, phys);
}

uint32_t kmalloc_ap(uint32_t size, uint32_t *phys) {
    return kmalloc_int(size, 1, phys);
}

uint32_t kmalloc(uint32_t size) {
    return kmalloc_int(size, 0, 0);
}

void *ksbrk(int incr) {
    if (program_break == 0 || program_break + incr >= program_break_end) return (void *) - 1;

    void *prev_break = program_break;
    program_break += incr;
    return prev_break;
}

// 寻找一个符合条件的指定大小的空闲内存块
static header_t *get_free_block(uint32_t size) {
    header_t *curr = head; // 从头开始
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size) return curr; // 空闲，并且大小也满足条件，直接返回
        curr = curr->s.next; // 下一位
    }
    return NULL; // 找不到
}

void *alloc(uint32_t size) {
    uint32_t total_size;
    void *block;
    header_t *header;
    if (!size) return NULL; // size == 0，自然不用返回
    header = get_free_block(size);
    if (header) { // 找到了对应的header！
        header->s.is_free = 0;
        return (void *) (header + 1);
        // header + 1，相当于把header的值在指针上后移了一个header_t，从而在返回的内存中不存在覆写header的现象
    }
    // 否则，申请内存
    total_size = sizeof(header_t) + size; // 需要一处放header的空间
    block = ksbrk(total_size); // sbrk，申请total_size大小内存
    if (block == (void *) -1) return NULL; // 没有，返回NULL
    // 申请成功！
    header = block; // 初始化header
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;
    if (!head) head = header; // 第一个还是空的，直接设为header
    if (tail) tail->s.next = header; // 有最后一个，把最后一个的next指向header
    tail = header; // header荣登最后一个
    return (void *) (header + 1); // 同上
}

void kfree(void *block) {
    header_t *header, *tmp;
    // 原文中用sbrk(0)获取program_break，但我们现在可以直接访问program_break
    if (!block) return; // free(NULL)，有什么用捏
    header = (header_t *) block - 1; // 减去一个header_t的大小，刚好指向header_t

    if ((char *) block + header->s.size == program_break) { // 正好在堆末尾
        if (head == tail) head = tail = NULL; // 只有一个内存块，全部清空
        else {
            // 遍历整个内存块链表，找到对应的内存块，并把它从链表中删除
            tmp = head;
            while (tmp) {
                // 如果内存在堆末尾，那这个块肯定也在链表末尾
                if (tmp->s.next == tail) { // 下一个就是原本末尾
                    tmp->s.next = NULL; // 踢掉
                    tail = tmp; // 末尾位置顶替
                }
                tmp = tmp->s.next; // 下一个
            }
        }
        // 释放这一块内存
        ksbrk(0 - sizeof(header_t) - header->s.size);
        return;
    }
    // 否则，设置为free
    header->s.is_free = 1;
}