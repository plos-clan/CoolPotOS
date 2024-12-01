#pragma once

#include "ctypes.h"

#define u_char unsigned char
#define u_long unsigned long
#define caddr_t size_t
#define bcopy(a, b, c) memcpy(b,a,c)

void *krealloc(void *cp, size_t nbytes);
void *kmalloc(size_t nbytes);
void *kcalloc(size_t nelem, size_t elsize);
void kfree(void *cp);
size_t kmalloc_usable_size(void *cp);
uint32_t get_kernel_memory_usage();