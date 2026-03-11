#include "sqfs/table.h"
#include "sqfs/error.h"

int sqfs_write_table(sqfs_file_t *file, sqfs_compressor_t *cmp,
                     const void *data, size_t table_size,
                     sqfs_u64 *start) {
    (void)file;
    (void)cmp;
    (void)data;
    (void)table_size;
    if (start != NULL)
        *start = 0;
    return SQFS_ERROR_UNSUPPORTED;
}
