#pragma once

#include "ctypes.h"

void *user_realloc(void *cp, size_t nbytes);
void *user_malloc(size_t nbytes);
void *user_calloc(size_t nelem, size_t elsize);
void user_free(void *cp);
size_t user_usable_size(void *cp);