#include "ctype.h"

extern uint64_t physical_memory_offset;

void init_hhdm();

void *phys_to_virt(uint64_t phys_addr);

uint64_t get_physical_memory_offset();

void *virt_to_phys(uint64_t virt_addr);
