#include "term/klog.h"
#include "krlibc.h"

static char   kmsg_buf[KMSG_SIZE];
static size_t kmsg_head = 0;
static size_t kmsg_tail = 0;

static int kmsg_empty(void) {
    return kmsg_head == kmsg_tail;
}

void kmsg_putc(char c) {
    kmsg_buf[kmsg_head] = c;
    kmsg_head           = (kmsg_head + 1) % KMSG_SIZE;
    if (kmsg_head == kmsg_tail) { kmsg_tail = (kmsg_tail + 1) % KMSG_SIZE; }
}

void kmsg_write(const char *s) {
    while (*s)
        kmsg_putc(*s++);
}

int kmsg_getc(void) {
    if (kmsg_head == kmsg_tail) return -1;
    char c    = kmsg_buf[kmsg_tail];
    kmsg_tail = (kmsg_tail + 1) % KMSG_SIZE;
    return c;
}

static size_t kmesg_read(int drive, uint8_t *buffer, size_t number, size_t lba) {
    UNUSED(drive, lba);

    size_t read_count = 0;
    while (read_count < number) {
        int c = kmsg_getc();
        if (c < 0) break; // 没有数据
        buffer[read_count++] = (uint8_t)c;
    }
    return read_count;
}