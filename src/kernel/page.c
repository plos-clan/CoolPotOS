#include "../include/memory.h"
#include "../include/graphics.h"
#include "../include/io.h"
#include "../include/task.h"
#include "../include/timer.h"

page_directory_t *kernel_directory = 0; // 内核用页目录
page_directory_t *current_directory = 0; // 当前页目录

uint32_t *frames;
uint32_t nframes;

extern struct task_struct *current;

extern uint32_t placement_address;
extern void *program_break, *program_break_end;

static void set_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / 0x1000;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames[idx] |= (0x1 << off);
}

static void clear_frame(uint32_t frame_addr) { //释放物理块
    uint32_t frame = frame_addr / 0x1000;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames[idx] &= ~(0x1 << off);
}

static uint32_t test_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / 0x1000;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    return (frames[idx] & (0x1 << off));
}

uint32_t first_frame() {
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
    if (page->present) {
        return;
    }
    else {
        uint32_t idx = first_frame();
        if (idx == (uint32_t) - 1) {
            printf("FRAMES_FREE_ERROR: Cannot free frames!\n");
            asm("cli");
            for (;;)io_hlt();
        }

        set_frame(idx * 0x1000);
        memset(page,0,4);
        page->present = 1; // 现在这个页存在了
        page->rw = is_writable ? 1 : 0; // 是否可写由is_writable决定
        page->user = is_kernel ? 0 : 1; // 是否为用户态由is_kernel决定
        page->frame = idx;
    }
}
void alloc_frame_line(page_t *page, uint32_t line,int is_kernel, int is_writable) {
    set_frame(line);
    memset(page,0,4);
    page->present = 1; // 现在这个页存在了
    page->rw = is_writable ? 1 : 0; // 是否可写由is_writable决定
    page->user = is_kernel ? 0 : 1; // 是否为用户态由is_kernel决定
    page->frame = line / 0x1000;

}

void free_frame(page_t *page) {
    uint32_t frame = page->frame;
    if (!frame) return;
    else {
        clear_frame(frame);
        page->frame = 0x0;
    }
}

void page_flush(page_directory_t *dir){
    asm volatile("invlpg (%0)" ::"r"(&dir->tablesPhysical));
    current_directory = dir;
    //asm volatile("mov %0, %%cr3" : : "r"(&dir->physicalAddr));
}

void page_switch(page_directory_t *dir){
    current_directory = dir;
    asm volatile("mov %0, %%cr3" : : "r"(&dir->tablesPhysical));

}

void switch_page_directory(page_directory_t *dir) {
    current_directory = dir;
    asm volatile("mov %0, %%cr3" : : "r"(&dir->tablesPhysical));

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

page_t *get_page(uint32_t address, int make, page_directory_t *dir,bool ist) {

    address /= 0x1000;

    uint32_t table_idx = address / 1024;

    if (dir->tables[table_idx]){
        page_t *pgg = &dir->tables[table_idx]->pages[address % 1024];
        return pgg;
    }
    else if (make) {
        uint32_t tmp;
        dir->tables[table_idx] = (page_table_t *) kmalloc_i_ap(sizeof(page_table_t), &tmp);
        memset(dir->tables[table_idx], 0, 0x1000);
        dir->tablesPhysical[table_idx] = tmp | 0x7;
        page_t *pgg = &dir->tables[table_idx]->pages[address % 1024];
        return pgg;
    } else return 0;
}


void page_fault(registers_t *regs) {
    io_cli();
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address)); //

    int present = !(regs->err_code & 0x1); // 页不存在
    int rw = regs->err_code & 0x2; // 只读页被写入
    int us = regs->err_code & 0x4; // 用户态写入内核页
    int reserved = regs->err_code & 0x8; // 写入CPU保留位
    int id = regs->err_code & 0x10; // 由取指引起

    printf("[ERROR]: Page fault |");
    if (present) {
        printf("Type: present;\n\taddress: %x  \n", faulting_address);
    } else if (rw) {
        printf("Type: read-only;\n\taddress: %x\n", faulting_address);
    } else if (us) {
        printf("Type: user-mode;\n\taddres: %x\n", faulting_address);
    } else if (reserved) {
        printf("Type: reserved;\n\taddress: %x\n", faulting_address);
    } else if (id) {
        printf("Type: decode address;\n\taddress: %x\n", faulting_address);
    }

    if (current->pid == 0) {
        printf(" ======= Kernel Error ======= \n");
        while (1) io_hlt();
    } else {
        task_kill(current->pid);
    }

    sleep(1);
    io_sti();
}

static page_table_t *clone_table(page_table_t *src, uint32_t *physAddr) {
    page_table_t *table = (page_table_t *) kmalloc_i_ap(sizeof(page_table_t), physAddr);
    memset(table, 0, sizeof(page_directory_t));

    int i;
    for (i = 0; i < 1024; i++) {
        if (!src->pages[i].frame)
            continue;
        alloc_frame(&table->pages[i], 0, 0);
        if (src->pages[i].present) table->pages[i].present = 1;
        if (src->pages[i].rw) table->pages[i].rw = 1;
        if (src->pages[i].user) table->pages[i].user = 1;
        if (src->pages[i].accessed)table->pages[i].accessed = 1;
        if (src->pages[i].dirty) table->pages[i].dirty = 1;
        copy_page_physical(src->pages[i].frame * 0x1000, table->pages[i].frame * 0x1000);
    }
    return table;
}

page_directory_t *clone_directory(page_directory_t *src) {
    uint32_t phys;
    page_directory_t *dir = (page_directory_t *) kmalloc_i_ap(sizeof(page_directory_t), &phys);
    memset(dir, 0, sizeof(page_directory_t));

    uint32_t offset = (uint32_t) dir->tablesPhysical - (uint32_t) dir;
    dir->physicalAddr = phys + offset;
    int i;
    for (i = 0; i < 1024; i++) {
        if (!src->tables[i])
            continue;
        if (kernel_directory->tables[i] == src->tables[i]) {
            dir->tables[i] = src->tables[i];
            dir->tablesPhysical[i] = src->tablesPhysical[i];
        } else {
            uint32_t phys;
            dir->tables[i] = clone_table(src->tables[i], &phys);
            dir->tablesPhysical[i] = phys | 0x07;
        }
    }
    return dir;
}

void init_page(multiboot_t *mboot) {
    uint32_t mem_end_page = 0xFFFFFFFF; // 4GB Page

    nframes = mem_end_page / 0x1000;
    frames = (uint32_t *) kmalloc(INDEX_FROM_BIT(nframes));
    memset(frames, 0, INDEX_FROM_BIT(nframes));

    uint32_t physical_addr;
    kernel_directory = (page_directory_t *) kmalloc_ap(sizeof(page_directory_t),&physical_addr); //kmalloc: 无分页情况自动在内核后方分配 | 有分页从内核堆分配
    kernel_directory->physicalAddr = physical_addr;

    memset(kernel_directory, 0, sizeof(page_directory_t));
    current_directory = kernel_directory;
    int i = 0;

    while (i < placement_address + 0x30000) {
        /*
         * 内核部分对ring3而言不可读不可写
         * 无偏移页表映射
         * 因为刚开始分配, 所以内核线性地址与物理地址对应
         */
        alloc_frame(get_page(i, 1, kernel_directory,false), 1, 1);
        i += 0x1000;
    }

    unsigned int j = mboot->framebuffer_addr,size = mboot->framebuffer_height * mboot->framebuffer_width*mboot->framebuffer_bpp;

    while (j <= mboot->framebuffer_addr + size){
        alloc_frame_line(get_page(j,1,kernel_directory,false),j,0,1);
        j += 0x1000;
    }

    for (int i = KHEAP_START; i < KHEAP_START + KHEAP_INITIAL_SIZE; i++) {
        alloc_frame(get_page(i, 1, kernel_directory,false), 1, 1);
    }


    register_interrupt_handler(14, page_fault);
    switch_page_directory(kernel_directory);

    program_break = (void *) KHEAP_START;
    program_break_end = (void *) (KHEAP_START + KHEAP_INITIAL_SIZE);

    klogf(true,"Memory manager is enable\n");
    printf("Kernel: 0x%08x | ",placement_address + 0x30000);
    printf("GraphicsBuffer: 0x%08x - 0x%08x \n",(mboot->framebuffer_addr),(mboot->framebuffer_addr + size));
    printf("KernelHeap: 0x%08x - 0x%08x | ",(KHEAP_START),(KHEAP_START + KHEAP_INITIAL_SIZE));
    printf("BaseFrame: 0x%08x\n",frames);
}
