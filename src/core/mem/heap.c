#include "heap.h"
#include "klog.h"
#include "hhdm.h"

void init_heap() {
    heap_init((uint8_t * )(physical_memory_offset + 0x3c0f000), 0x400000);
}
