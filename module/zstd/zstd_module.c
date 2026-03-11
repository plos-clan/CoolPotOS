#include "cp_kernel.h"
#include "cpzstd.h"
#include "errno.h"
#include "zstd.h"
#include "zstd_errors.h"

__attribute__((used)) __attribute__((visibility("default"))) size_t
cpzstd_decompress(void *dst, size_t dst_capacity, const void *src, size_t src_size) {
    return ZSTD_decompress(dst, dst_capacity, src, src_size);
}

__attribute__((used)) __attribute__((visibility("default"))) bool cpzstd_is_error(size_t code) {
    return ZSTD_isError(code) != 0;
}

__attribute__((used)) __attribute__((visibility("default"))) int cpzstd_get_error_code(size_t code) {
    return ZSTD_getErrorCode(code);
}

__attribute__((used)) __attribute__((visibility("default"))) const char *
cpzstd_get_error_name(size_t code) {
    return ZSTD_getErrorName(code);
}

__attribute__((used)) __attribute__((visibility("default"))) int dlmain(void) {
    return EOK;
}
