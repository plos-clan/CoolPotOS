#include "free_page.h"
#include "kmalloc.h"
#include "fifo8.h"

struct FIFO8 *fifo8;

void put_directory(page_directory_t *dir){
    fifo8_put(fifo8,(uint32_t)dir);
}

void free_pages(){
    int ii;
    do{
        ii = fifo8_get(fifo8);
        if(ii == -1) return;
        page_directory_t *dir = (page_directory_t *)ii;
        return;
        for (int i = 0; i < 1024; i++) {
            page_table_t *table = dir->tables[i];
            for (int j = 0; j < 1024; j++) {
                page_t page = table->pages[i];
                free_frame(&page);
            }
           // kfree(table);
        }
        //kfree(dir);
    } while (1);
}

/*
 * 用于回收进程创建的页表项, 由于进程退出过程中还是在该进程的页表中无法直接回收
 * 故移动到内核IDLE进程统一回收处理
*/
void setup_free_page(){
    fifo8 = kmalloc(sizeof(struct FIFO8));
    uint8_t *buf = kmalloc(sizeof(uint32_t) * MAX_FREE_QUEUE);
    fifo8_init(fifo8,sizeof(uint32_t) * MAX_FREE_QUEUE,buf);
}

uint32_t kh_usage_memory_byte = 0;

uint32_t get_kernel_memory_usage(){
    return kh_usage_memory_byte;
}
