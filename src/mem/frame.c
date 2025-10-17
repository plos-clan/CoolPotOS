#include "mem/frame.h"
#include "mem/buddy.h"
#include "mem/page.h"
#include "limine.h"
#include "bootarg.h"
#include "krlibc.h"

LIMINE_REQUEST struct limine_memmap_request memmap_request = {
    .id       = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

LIMINE_REQUEST struct limine_hhdm_request hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

static uint64_t physical_memory_offset;
FrameAllocator  frame_allocator;

uint64_t get_memory_size() {
    uint64_t                       all_memory_size = 0;
    struct limine_memmap_response *memory_map      = memmap_request.response;

    for (uint64_t i = memory_map->entry_count - 1;; i--) {
        struct limine_memmap_entry *region = memory_map->entries[i];
        if (region->type == LIMINE_MEMMAP_USABLE) {
            all_memory_size = region->base + region->length;
            break;
        }
    }
    return all_memory_size;
}

uint64_t mem_parse_size(const char *s) {
    if (s == NULL) { return UINT64_MAX; }

    uint8_t *p     = (uint8_t*)s;
    uint64_t value = 0;

    while (isdigit((*p))) {
        int digit = *p - '0';
        value     = value * 10 + digit;
        p++;
    }
    if (*p == '\0') { return value; }
    char suffix = *p;

    if (suffix >= 'a' && suffix <= 'z') { suffix -= ('a' - 'A'); }

    switch (suffix) {
    case 'K':
        value *= KILO_FACTOR; // 乘以 1024 (2^10)
        break;
    case 'M':
        value *= MEGA_FACTOR; // 乘以 1048576 (2^20)
        break;
    case 'G':
        value *= GIGA_FACTOR; // 乘以 1073741824 (2^30)
        break;
    default: return value;
    }
    if (*(p + 1) != '\0') { return UINT64_MAX; }

    return value;
}

void init_frame() {
    physical_memory_offset = hhdm_request.response->offset;
    const char    *mem_str = boot_get_cmdline_param("mem");
    uint64_t memory_size = 0;

    if(mem_str == NULL) goto Ldefault;
    if (strcmp(mem_str, "default") != 0) {
        memory_size = mem_parse_size(mem_str);
        memory_size =
            memory_size == UINT64_MAX ? get_memory_size() : PADDING_DOWN(memory_size, PAGE_SIZE);
    } else {
    Ldefault:
        memory_size = get_memory_size();
    }

    init_frame_buddy(memory_size);
}

struct limine_memmap_response *get_memory_map() {
    return memmap_request.response;
}

uint64_t get_physical_memory_offset() {
    return physical_memory_offset;
}

void *phys_to_virt(uint64_t phys_addr) {
    if (phys_addr == 0) return NULL;
    return (void *)(phys_addr + physical_memory_offset);
}

uint64_t virt_to_phys(void *virt_addr) {
    if (virt_addr == 0) return 0;
    return (uint64_t)(virt_addr - physical_memory_offset);
}
