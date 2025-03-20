#include "page.h"
#include "acpi.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kmalloc.h"
#include "krlibc.h"
#include "scheduler.h"

void copy_page_physical(uint32_t, uint32_t);

volatile uint32_t *frames;
volatile uint32_t  nframes;

page_directory_t *kernel_directory;
page_directory_t *current_directory = 0;

extern void *program_break;
extern void *program_break_end;

extern HpetInfo *hpetInfo;

static void set_frame(uint32_t frame_addr) {  //设置物理块为占用
    uint32_t frame  = frame_addr / PAGE_SIZE; //计算内存块编号
    uint32_t idx    = INDEX_FROM_BIT(frame);  //计算索引
    uint32_t off    = OFFSET_FROM_BIT(frame); //计算偏移
    frames[idx]    |= (0x1U << off);          //标记占用
}

static void clear_frame(uint32_t frame_addr) { //释放物理块
    uint32_t frame  = frame_addr / PAGE_SIZE;  //计算内存块编号
    uint32_t idx    = INDEX_FROM_BIT(frame);   //计算索引
    uint32_t off    = OFFSET_FROM_BIT(frame);  //计算偏移
    frames[idx]    &= ~(0x1U << off);          //释放内存块
}

static uint32_t test_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / PAGE_SIZE; //计算内存块编号
    uint32_t idx   = INDEX_FROM_BIT(frame);  //计算索引
    uint32_t off   = OFFSET_FROM_BIT(frame); //计算偏移
    return (frames[idx] & (0x1U << off));    //检查物理内存块状态
}

uint32_t first_frame() {                                               //获取第一个空闲物理块
    for (int i = 0; i < INDEX_FROM_BIT(0xFFFFFFFF / PAGE_SIZE); i++) { //遍历物理内存块
        if (frames[i] != 0xffffffff) {                                 //检查当前内存块是否已分配
            for (int j = 0; j < 32; j++) {                             //遍历当前内存块的每一位
                uint32_t toTest = 0x1U << j;                           //检查当前位是否为空闲
                if (!(frames[i] & toTest)) { //找到第一个空闲内存块，计算并返回编号
                    return i * 4 * 8 + j;
                }
            }
        }
    }
    return (uint32_t)-1; //无空闲内存块，报错
}

void free_frame(page_t *page) {
    uint32_t frame = page->frame;
    if (!frame)
        return; //检查页面帧是否有效
    else {
        page->present = 0; //释放页面帧
        clear_frame(frame);
        page->frame = 0x0;
    }
}

void alloc_frame(page_t *page, int is_kernel, int is_writable) {
    if (page->present) { //检查页面是否已分配
        return;
    } else {
        uint32_t idx = first_frame();
        if (idx == (uint32_t)-1) { //若无空闲的物理内存帧，返回报错
            printk("FRAMES_FREE_ERROR: Cannot free frames!\n");
            __asm__("cli"); //阻塞内核运行
            for (;;)
                io_hlt();
        }

        set_frame(idx * PAGE_SIZE);          //计算物理内存帧的地址并标记为已分配
        memset(page, 0, 4);                  //清零前4个字节，确保页面干净
        page->present = 1;                   // 表示该页已经分配了内存帧
        page->rw      = is_writable ? 1 : 0; //根据is_writable决定是否可写
        page->user    = is_kernel ? 0 : 1;   // 根据is_kernel决定是否为用户态
        page->frame   = idx;                 //设置该页面关联的物理内存帧的编号
    }
}

void alloc_frame_line(page_t *page, uint32_t line, int is_kernel, int is_writable) {
    set_frame(line);
    memset(page, 0, 4);

    page->present = 1;                   //标记内存已被分配
    page->rw      = is_writable ? 1 : 0; // 根据is_writable决定是否可写
    page->user    = is_kernel ? 0 : 1;   // 根据is_kernel决定是否为用户态
    page->frame   = line / PAGE_SIZE;    //设置该页面关联的物理内存帧的编号
}

page_t *get_page(uint32_t address, int make, page_directory_t *dir) {

    address /= PAGE_SIZE; //计算页面在页面表中的索引

    uint32_t table_idx = address / 1024; //计算页面目录中的页面表索引

    if (dir->tables[table_idx]) { //检查页面表是否存在
        volatile page_t *pgg = &dir->tables[table_idx]->pages[address % 1024];
        return (page_t *)pgg; //返回原页面表参数
    } else if (make) {        //创造新的页面表
        uint32_t tmp;
        tmp = (uint32_t)(dir->tables[table_idx] = (page_table_t *)kmalloc(sizeof(page_table_t)));
        memset((void *)dir->tables[table_idx], 0, PAGE_SIZE);
        dir->table_phy[table_idx] = tmp | 0x7;
        volatile page_t *pgg      = &dir->tables[table_idx]->pages[address % 1024];
        return (page_t *)pgg;
    } else
        return 0; //无法获取页面表项
}

static void open_page() { //打开分页机制
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=b"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "b"(cr0));
}

void switch_page_directory(page_directory_t *dir) {
    current_directory = dir;                                    //更新当前页面目录
    __asm__ volatile("mov %0, %%cr3" : : "r"(&dir->table_phy)); //切换页面目录
}

static page_table_t *clone_table(page_table_t *src, uint32_t *physAddr) {
    page_table_t *table = (page_table_t *)kmalloc(sizeof(page_table_t)); //分配新页面表
    *physAddr           = (uint32_t)table;
    memset(table, 0, sizeof(page_directory_t)); //初始化新页面表

    int i;
    for (i = 0; i < 1024; i++) { //遍历原页面表
        if (!src->pages[i].frame) continue;
        alloc_frame(&table->pages[i], 0, 0);
        if (src->pages[i].present) table->pages[i].present = 1; //设置相关属性
        if (src->pages[i].rw) table->pages[i].rw = 1;
        if (src->pages[i].user) table->pages[i].user = 1;
        if (src->pages[i].accessed) table->pages[i].accessed = 1;
        if (src->pages[i].dirty) table->pages[i].dirty = 1;
        copy_page_physical(src->pages[i].frame * 0x1000, table->pages[i].frame * 0x1000);
    }

    return table; //返回新页面表
}

page_directory_t *clone_directory(page_directory_t *src) {
    page_directory_t *dir = (page_directory_t *)kmalloc(sizeof(page_directory_t)); //分配新页面目录

    memset(dir, 0, sizeof(page_directory_t)); //初始化新页面目录

    int i;
    for (i = 0; i < 1024; i++) { //遍历原页面目录
        if (!src->tables[i])     //该页面目录项未分配页面表，跳过当前循环
            continue;
        if (kernel_directory->tables[i] ==
            src->tables[i]) { //源页面目录项的页面表与内核页面目录的页面表相同，直接共享页面表
            dir->tables[i]    = src->tables[i];
            dir->table_phy[i] = src->table_phy[i];
        } else { //源页面目录项的页面表与内核页面目录的页面表不同，克隆页面表
            uint32_t phys;
            dir->tables[i]    = clone_table(src->tables[i], &phys); //设置页面目录项的属性
            dir->table_phy[i] = phys | 0x07;
        }
    }
    return dir;
}

page_directory_t *get_current_directory() {
    return current_directory; //返回当前页面目录
}

void page_fault(registers_t *regs) {
    io_cli();
    disable_scheduler();
    uint32_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));

    int present  = !(regs->err_code & 0x1); // 页不存在
    int rw       = regs->err_code & 0x2;    // 只读页被写入
    int us       = regs->err_code & 0x4;    // 用户态写入内核页
    int reserved = regs->err_code & 0x8;    // 写入CPU保留位
    int id       = regs->err_code & 0x10;   // 由取指引起

    logkf("eip: %08x | esp: %08x | peip: %08x | pesp: %08x\n", regs->eip, regs->esp,
          get_current_proc()->context.eip, get_current_proc()->context.esp);

    logk("[Error]: Page fault ");
    logkf("[%s]", get_current_proc() != NULL ? get_current_proc()->name : "Not Process");
    logkf(" Type: %s ",
          (present ? "present"
                   : (rw ? "read-only"
                         : (us ? "user-mode"
                               : (reserved ? "reserved" : (id ? "decode address" : "unknown"))))));
    logkf("address: %x \n",
          faulting_address); //调试: 串口优先打印信息, 屏蔽虚拟终端的BUG

    if (get_current_proc() == NULL) {
        printk("[ERROR]: Page fault [Not Process] |");
    } else
        printk("[ERROR]: Page fault [PID: %d |Level: %d] |", get_current_proc()->pid,
               get_current_proc()->task_level);
    if (present) {
        printk("Type: present | address: %x  \n", faulting_address);
    } else if (rw) {
        printk("Type: read-only | address: %x\n", faulting_address);
    } else if (us) {
        printk("Type: user-mode | addres: %x\n", faulting_address);
    } else if (reserved) {
        printk("Type: reserved | address: %x\n", faulting_address);
    } else if (id) {
        printk("Type: decode address | address: %x\n", faulting_address);
    }
    if (get_current_proc() == NULL) {
        klogf(false, "Kernel Error Panic.\n");
        while (1)
            io_hlt();
    }

    // 多进程相关, 未实现多进程的OS可将以下代码全部替换为 while(1)
    if (get_current_proc()->task_level == TASK_KERNEL_LEVEL) {
        klogf(false, "Kernel Error Panic.\n");
        while (1)
            io_hlt();
    } else if (get_current_proc()->task_level == TASK_APPLICATION_LEVEL) {
        kill_proc(get_current_proc());
    } else if (get_current_proc()->task_level == TASK_SYSTEM_SERVICE_LEVEL) {
        //TODO 蓝屏处理程序待实现
        klogf(false, "System service error. ==Kernel Panic==\n");
        while (1)
            io_hlt();
    }

    enable_scheduler();
    io_sti();
}

void page_init(multiboot_t *multiboot) {
    uint32_t mem_end_page = 0xFFFFFFFF; // 4GB Page
    nframes               = mem_end_page / PAGE_SIZE;

    frames = (uint32_t *)kmalloc(INDEX_FROM_BIT(nframes));
    memset(frames, 0, INDEX_FROM_BIT(nframes));

    kernel_directory = (page_directory_t *)kmalloc(sizeof(page_directory_t));
    memset(kernel_directory, 0, sizeof(page_directory_t));

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

    uint32_t j = multiboot->framebuffer_addr, size = multiboot->framebuffer_height *
                                                     multiboot->framebuffer_width *
                                                     multiboot->framebuffer_bpp;
    while (j <= multiboot->framebuffer_addr + size) { //VBE显存缓冲区映射
        alloc_frame_line(get_page(j, 1, kernel_directory), j, 0, 1);
        j += 0x1000;
    }

    register_interrupt_handler(14, page_fault);
    switch_page_directory(kernel_directory);
    open_page();
}