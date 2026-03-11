#pragma once

#include "cp_kernel.h"

size_t      cpzstd_decompress(void *dst, size_t dst_capacity, const void *src, size_t src_size);
bool        cpzstd_is_error(size_t code);
int         cpzstd_get_error_code(size_t code);
const char *cpzstd_get_error_name(size_t code);
