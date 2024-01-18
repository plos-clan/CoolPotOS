#include "../include/task.h"
#include "../include/memory.h"
#include "../include/config.h"
#include "../include/vga.h"
#include "../include/io.h"
#include "../include/description_table.h"

task_t *curror_task;
struct tss_entry tss_e;

void tss_install() {
    asm volatile("ltr %%ax" : : "a"((SEL_TSS << 3)));
}

void tss_set(uint16_t ss0, uint32_t esp0) {
    // 清空TSS
    memset((void *)&tss_e, 0, sizeof(struct tss_entry));
    tss_e.ss0 = ss0;
    tss_e.esp0 = esp0;
    tss_e.iopb_off = sizeof(struct tss_entry);
}

// 重置当前进程的TSS
void tss_reset() {
    // TSS用于当切换到ring0时设置堆栈
    // 每个进程有一个内核堆栈
    tss_set(SEL_KDATA << 3, (uint32_t)curror_task->stack + PAGE_SIZE);
}

void init_task(){
    io_cli();
    
    io_sti();
}