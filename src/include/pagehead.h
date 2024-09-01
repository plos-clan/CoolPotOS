#ifndef CRASHPOWEROS_PAGEHEAD_H
#define CRASHPOWEROS_PAGEHEAD_H

#include <stdint.h>
#include <stddef.h>

void *page_sbrk(int incr);
void *page_alloc(size_t size);
void page_free(void *block);
void *page_malloc(size_t size,uint32_t *phys);

#endif
