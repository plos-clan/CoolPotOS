#include "heap.h"
#include "klog.h"
#include "hhdm.h"
#include "krlibc.h"

uint64_t get_all_memusage(){
    return 0x400000;
}

/*
 * 跟智障的编译器优化斗智斗勇的产物
 */
void* calloc(size_t a,size_t b){
    void *p =  malloc(a * b);
    memset(p,0,a * b);
    return p;
}

void init_heap() {
    heap_init((uint8_t * )(physical_memory_offset + 0x3c0f000), 0x400000);
}
