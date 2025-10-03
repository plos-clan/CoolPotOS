#pragma once

#include "ctype.h"
#include "pcb.h"

void *user_realloc(pcb_t *pcb, void *cp, size_t nbytes);

void *user_malloc(pcb_t *pcb, size_t nbytes);

void *user_calloc(size_t nelem, size_t elsize);

void user_free(void *cp);

size_t user_usable_size(void *cp);