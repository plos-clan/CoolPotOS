#include "elf_util.h"
#include "klog.h"
#include "page.h"
#include "krlibc.h"

void segment_callback(struct ElfSegment segment) {
    page_directory_t *pg_dir = get_current_directory();
    for (size_t i = segment.address; i < segment.address + segment.size; i+=PAGE_SIZE) {
        alloc_frame(get_page(i,1,pg_dir),0,1);
    }
    memcpy((void*)segment.address,segment.data,segment.size);
}

uint32_t elf_load(size_t elf_size,uint8_t *elf_data) {
    struct ElfParseResult result = parse_elf(elf_data, elf_size, segment_callback);
    switch (result.tag) {
        case EntryPoint:
            return result.entry_point;
        case InvalidElfData:
            printk("Invalid ELF data.\n");
            break;
        case ElfContainsNoSegments:
            printk("ELF contains no segments.\n");
            break;
        case FailedToGetSegmentData:
            printk("Failed to get segment data.\n");
            break;
        case AllocFunctionNotProvided:
            printk("Allocation function not provided.\n");
            break;
        default:
            printk("Unknown error.\n");
            break;
    }
    return 0;
}
