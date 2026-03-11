#include "config.h"
#include "util/util.h"
#include "errno.h"
extern errno_t errno;

void *alloc_flex(size_t base_size, size_t item_size, size_t nmemb) {
    size_t size;

    if (SZ_MUL_OV(nmemb, item_size, &size) || SZ_ADD_OV(base_size, size, &size)) {
        errno = EOVERFLOW;
        return NULL;
    }

    return calloc(1, size);
}

void *alloc_array(size_t item_size, size_t nmemb) {
    size_t size;

    if (SZ_MUL_OV(nmemb, item_size, &size)) {
        errno = EOVERFLOW;
        return NULL;
    }

    return calloc(1, size);
}
