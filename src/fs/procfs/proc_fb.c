#include "boot.h"
#include "driver/fb.h"
#include "fs/procfs.h"
#include "krlibc.h"

static char *proc_gen_fb(size_t *content_len) {
    char *content = calloc(1, 64);
    if (content == NULL) {
        *content_len = 0;
        return NULL;
    }

    if (boot_framebuffer_count() == 0 || boot_get_framebuffer(0) == NULL) {
        *content_len = 0;
        return content;
    }

    const int len = snprintf(content, 64, "0 %s\n", FB_DEVICE_NAME);
    *content_len  = len > 0 ? (size_t)len : 0;
    return content;
}

size_t proc_fb_stat(proc_handle_t *handle) {
    UNUSED(handle);

    size_t len   = 0;
    char *output = proc_gen_fb(&len);
    free(output);
    return len;
}

size_t proc_fb_read(proc_handle_t *handle, void *addr, size_t offset, size_t size) {
    UNUSED(handle);

    size_t len   = 0;
    char *output = proc_gen_fb(&len);
    if (output == NULL || offset >= len) {
        free(output);
        return 0;
    }

    size_t actual = len - offset;
    if (actual > size) {
        actual = size;
    }

    memcpy(addr, output + offset, actual);
    free(output);
    return actual;
}
