#include "../include/memory.h"
#include "../include/vga.h"
#include "../include/io.h"

page_directory_t *kernel_directory = 0; // 内核用页目录
page_directory_t *current_directory = 0; // 当前页目录

uint32_t *frames;
uint32_t nframes; 

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
}

static uint32_t first_frame() 
{
    for (int i = 0; i < INDEX_FROM_BIT(nframes); i++) {
        if (frames[i] != 0xffffffff) { 
            for (int j = 0; j < 32; j++) { 
                uint32_t toTest = 0x1 << j; 
                if (!(frames[i] & toTest)) {
                    return i * 4 * 8 + j; 
                }
            }
        }
    }
    return (uint32_t) - 1; 
}

void alloc_frame(page_t *page, int is_kernel, int is_writable) {
    if (page->frame) return; 
    else {
        uint32_t idx = first_frame();
        if (idx == (uint32_t) - 1) {
            printf("FRAMES_FREE_ERROR: Cannot free frames!\n");
			asm("cli");
			for(;;)io_hlt();
        }
        set_frame(idx * 0x1000); 
        page->present = 1; // 现在这个页存在了
        page->rw = is_writable ? 1 : 0; // 是否可写由is_writable决定
        page->user = is_kernel ? 0 : 1; // 是否为用户态由is_kernel决定
        page->frame = idx; 
    }
}

void free_frame(page_t *page) {
    uint32_t frame = page->frame;
    if (!frame) return;
    else {
        clear_frame(frame); 
        page->frame = 0x0; 
    }
}

void init_page() {
    uint32_t mem_end_page = 0xFFFFFFFF; // 4GB Page

    nframes = mem_end_page / 0x1000; 
    frames = (uint32_t *) kmalloc(INDEX_FROM_BIT(nframes));
    memset(frames, 0, INDEX_FROM_BIT(nframes)); 

    kernel_directory = (page_directory_t *) kmalloc_a(sizeof(page_directory_t)); //kmalloc: 无分页情况自动在内核后方分配 | 有分页从内核堆分配

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
    address /= 0x1000; 
    uint32_t table_idx = address / 1024; 
    if (dir->tables[table_idx]) return &dir->tables[table_idx]->pages[address % 1024]; 
    else if (make) { 
        uint32_t tmp;
        dir->tables[table_idx] = (page_table_t *) kmalloc_ap(sizeof(page_table_t), &tmp); 
        memset(dir->tables[table_idx], 0, 0x1000); 
        dir->tablesPhysical[table_idx] = tmp | 0x7; 
        return &dir->tables[table_idx]->pages[address % 1024]; 
    } else return 0; 
}

void page_fault(registers_t *regs) {
    // 现在一个#PE错误出现，根据标准，错误地址在cr2寄存器中。
	asm("cli");
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address)); //
   
    int present = !(regs->err_code & 0x1); // 页不存在
    int rw = regs->err_code & 0x2; // 只读页被写入
    int us = regs->err_code & 0x4; // 用户态写入内核页
    int reserved = regs->err_code & 0x8; // 写入CPU保留位
    int id = regs->err_code & 0x10; // 由取指引起

    printf("[ERROR]: Page fault |");
	if(present)
		printf("Type: present;\n\taddress: %x  ",faulting_address);
	else if (rw)
		printf("Type: read-only;\n\taddress: %x",faulting_address);
	else if (us)
		printf("Type: user-mode;\n\taddres: %x",faulting_address);
	else if(reserved)
		printf("Type: reserved;\n\taddress: %x",faulting_address);
	else if(id)
		printf("Type: decode address;\n\taddress: %x",faulting_address);
	

	for(;;) io_hlt();
}

