#include "heap.h"
#include "klog.h"
#include "hhdm.h"
#include "krlibc.h"

uint64_t get_all_memusage(){
    return 0x400000;
}

void init_heap() {
    heap_init((uint8_t * )(physical_memory_offset + 0x3c0f000), 0x400000);
}
