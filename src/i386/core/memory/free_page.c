#include "free_page.h"
#include "fifo8.h"
#include "klog.h"
#include "kmalloc.h"
#include "scheduler.h"

struct FIFO8 *fifo8;

void put_directory(page_directory_t *dir) {
    fifo8_put(fifo8, (uint32_t)dir);
}

void free_pages() {
    disable_scheduler();
    uint32_t ii;
    do {
        ii = fifo8_get(fifo8);
        if (ii == -1 || ii == 0) { break; }
        page_directory_t *dir = (page_directory_t *)ii;
        for (int i = 0; i < 1024; i++) {
            page_table_t *table = dir->tables[i];
            if (table == NULL) continue;
            for (int j = 0; j < 1024; j++) {
                page_t page = table->pages[j];
                // TODO:此处存疑，此处数组下标应为j，源代码为i，猜测由于二者皆递增且范围一样，没有出现问题，理论上会导致没法遍历整个内存表，因环境问题无法测试，作此标注
                free_frame(&page);
            }
            kfree(table);
        }
        kfree(dir);
    } while (1);
    enable_scheduler();
}

uint32_t get_all_memusage() { // 获取总内存占用
    extern void *program_break;
    extern void *program_break_end;
    uint32_t     bytes = (uint32_t)program_break_end - (uint32_t)program_break; // 获取内核内存占用

    extern pcb_t *running_proc_head;
    pcb_t        *pcb = running_proc_head;
    while (pcb != NULL) { // 累加每个进程的内存占用
        pcb = pcb->next;
        if (pcb->task_level != TASK_KERNEL_LEVEL) {
            bytes += (uint32_t)pcb->program_break_end - (uint32_t)pcb->program_break;
            bytes += pcb->exe_file->size;
        }
    }
    return bytes;
}

/*
 * 用于回收进程创建的页表项, 由于进程退出过程中还是在该进程的页表中无法直接回收
 * 故移动到内核IDLE进程统一回收处理
*/
void setup_free_page() { // 初始化列表释放队列
    fifo8        = kmalloc(sizeof(struct FIFO8));
    uint8_t *buf = kmalloc(sizeof(uint32_t) * MAX_FREE_QUEUE);
    fifo8_init(fifo8, sizeof(uint32_t) * MAX_FREE_QUEUE, buf);
}
