#include "../../include/memory.h"
#include "../../include/isr.h"
#include "../../include/io.h"
#include "../../include/acpi.h"
#include "../../include/printf.h"
#include "../../include/task.h"
#include "../../include/timer.h"

extern uint32_t end; //linker.ld 内核末尾地址

volatile uint32_t *frames;
volatile uint32_t nframes;

extern uint32_t placement_address;

page_directory_t *kernel_directory;
page_directory_t *current_directory = 0;

extern void *program_break;
extern void *program_break_end;

extern HpetInfo *hpetInfo;

static void set_frame(uint32_t frame_addr) { //设置物理块为占用
    uint32_t frame = frame_addr / PAGE_SIZE;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames[idx] |= (0x1 << off);
}

static void clear_frame(uint32_t frame_addr) { //释放物理块
    uint32_t frame = frame_addr / PAGE_SIZE;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames[idx] &= ~(0x1 << off);
}

static uint32_t test_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / PAGE_SIZE;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    return (frames[idx] & (0x1 << off));
}

uint32_t first_frame() { //获取第一个空闲物理块
    for (int i = 0; i < INDEX_FROM_BIT(0xFFFFFFFF / PAGE_SIZE); i++) {
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

void free_frame(page_t *page) {
    uint32_t frame = page->frame;
    if (!frame) return;
    else {
        page->present = 0;
        clear_frame(frame);
        page->frame = 0x0;
    }
}

void alloc_frame(page_t *page, int is_kernel, int is_writable) {
    if (page->present) {
        return;
    }
    else {
        uint32_t idx = first_frame();
        if (idx == (uint32_t) - 1) {
            printk("FRAMES_FREE_ERROR: Cannot free frames!\n");
            asm("cli");
            for (;;)io_hlt();
        }

        set_frame(idx * PAGE_SIZE);
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
    page->frame = line / PAGE_SIZE;
}

page_t *get_page(uint32_t address, int make, page_directory_t *dir) {

    address /= PAGE_SIZE;

    uint32_t table_idx = address / 1024;

    if (dir->tables[table_idx]){
        page_t *pgg = &dir->tables[table_idx]->pages[address % 1024];
        return pgg;
    }else if (make) {
        uint32_t tmp;
        tmp = (uint32_t )(dir->tables[table_idx] = (page_table_t*) kmalloc(sizeof(page_table_t)));
        memset(dir->tables[table_idx], 0, PAGE_SIZE);
        dir->table_phy[table_idx] = tmp | 0x7;
        page_t *pgg = &dir->tables[table_idx]->pages[address % 1024];
        return pgg;
    } else return 0;
}

static void open_page(){ //打开分页机制
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=b"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "b"(cr0));
}

void switch_page_directory(page_directory_t *dir){
    current_directory = dir;
    asm volatile("mov %0, %%cr3" : : "r"(&dir->table_phy));
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

    printk("[ERROR]: Page fault |");
    if (present) {
        printk("Type: present;\n\taddress: %x  \n", faulting_address);
    } else if (rw) {
        printk("Type: read-only;\n\taddress: %x\n", faulting_address);
    } else if (us) {
        printk("Type: user-mode;\n\taddres: %x\n", faulting_address);
    } else if (reserved) {
        printk("Type: reserved;\n\taddress: %x\n", faulting_address);
    } else if (id) {
        printk("Type: decode address;\n\taddress: %x\n", faulting_address);
    }

    if (get_current()->pid == 0) {
        printf(" ======= Kernel Error ======= \n");
        while (1) io_hlt();
    } else {
        task_kill(get_current()->pid);
    }

    sleep(1);
    while (1) io_hlt();
}

static page_table_t *clone_table(page_table_t *src, uint32_t *physAddr) {
    page_table_t *table = (page_table_t *) kmalloc(sizeof(page_table_t));
    *physAddr = table;
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
    page_directory_t *dir = (page_directory_t *) kmalloc(sizeof(page_directory_t));

    memset(dir, 0, sizeof(page_directory_t));

    int i;
    for (i = 0; i < 1024; i++) {
        if (!src->tables[i])
            continue;
        if (kernel_directory->tables[i] == src->tables[i]) {
            dir->tables[i] = src->tables[i];
            dir->table_phy[i] = src->table_phy[i];
        } else {
            uint32_t phys;
            dir->tables[i] = clone_table(src->tables[i], &phys);
            dir->table_phy[i] = phys | 0x07;
        }
    }
    return dir;
}

uint32_t memory_usage(){
    return 0;
}

void init_page(multiboot_t *multiboot){
    uint32_t mem_end_page = 0xFFFFFFFF; // 4GB Page
    nframes = mem_end_page / PAGE_SIZE;

    program_break_end = program_break + 0x300000 + 1 + KHEAP_INITIAL_SIZE;

    frames = (uint32_t *) kmalloc(INDEX_FROM_BIT(nframes));
    memset(frames, 0, INDEX_FROM_BIT(nframes));

    kernel_directory = (page_directory_t *) kmalloc(sizeof(page_directory_t));
    memset(kernel_directory, 0, sizeof(page_directory_t));
    current_directory = kernel_directory;

    for (int index = 0; index < 1024; index++) { //页目录滞空
        kernel_directory->table_phy[index] = 0x00000002;
    }

    int i = 0;

    while (i < (int)program_break_end) {
        /*
         * 内核部分对ring3而言不可读不可写
         * 无偏移页表映射
         * 因为刚开始分配, 所以内核线性地址与物理地址对应
         */
        page_t *p = get_page(i, 1, kernel_directory);
        alloc_frame(p, 1, 1);
        i += PAGE_SIZE;
    }

    for (uint32_t j = (uint32_t )hpetInfo; j < ((uint32_t)hpetInfo)+ sizeof(HpetInfo); j+= PAGE_SIZE) {
        alloc_frame_line(get_page(j,1,kernel_directory),j,1,0);
    }

    uint32_t j = multiboot->framebuffer_addr,
            size = multiboot->framebuffer_height * multiboot->framebuffer_width*multiboot->framebuffer_bpp;

    while (j <= multiboot->framebuffer_addr + size){
        alloc_frame_line(get_page(j,1,kernel_directory),j,0,1);
        j += 0x1000;
    }

    register_interrupt_handler(14, page_fault);
    switch_page_directory(kernel_directory);
    open_page();
}