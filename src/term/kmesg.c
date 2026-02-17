#include "krlibc.h"
#include "lock.h"
#include "term/klog.h"

static char kmsg_buf[KMSG_SIZE];
static size_t kmsg_head = 0;
static size_t kmsg_tail = 0;
spin_t kmsg_lock = SPIN_INIT;

void kmsg_empty(void) {
    kmsg_head = 0;
    kmsg_tail = 0;
    memset(kmsg_buf, 0, KMSG_SIZE);
}

void kmsg_putc(char c) {
    kmsg_buf[kmsg_head] = c;
    kmsg_head = (kmsg_head + 1) % KMSG_SIZE;
    if (kmsg_head == kmsg_tail) {
        kmsg_tail = (kmsg_tail + 1) % KMSG_SIZE;
    }
}

void kmsg_write(const char *s) {
    spin_lock(kmsg_lock);
    while (*s)
        kmsg_putc(*s++);
    spin_unlock(kmsg_lock);
}

int kmsg_getc(void) {
    if (kmsg_head == kmsg_tail)
        return -1;
    char c = kmsg_buf[kmsg_tail];
    kmsg_tail = (kmsg_tail + 1) % KMSG_SIZE;
    return c;
}

size_t kmesg_read(uint8_t *buffer, size_t length) {
    spin_lock(kmsg_lock);
    size_t read_count = 0;
    while (read_count < length) {
        int c = kmsg_getc();
        if (c < 0)
            break; // 没有数据
        buffer[read_count++] = (uint8_t)c;
    }
    spin_unlock(kmsg_lock);
    return read_count;
}

size_t kmsg_length() {
    if (kmsg_head >= kmsg_tail) {
        return kmsg_head - kmsg_tail;
    } else {
        return KMSG_SIZE - kmsg_tail + kmsg_head;
    }
}

static int kmsg_getc_at(size_t offset) {
    size_t len = kmsg_length();
    if (offset >= len) {
        return -1; // 偏移量超出未读数据范围
    }

    // 计算要读取的缓冲区索引
    size_t index = (kmsg_tail + offset) % KMSG_SIZE;

    return kmsg_buf[index];
}

size_t kmsg_read_all(uint8_t *buffer, size_t length) {
    spin_lock(kmsg_lock);

    size_t read_count = 0;
    size_t data_len = kmsg_length();

    size_t count_to_read = (length < data_len) ? length : data_len;

    while (read_count < count_to_read) {
        int c = kmsg_getc_at(read_count);
        buffer[read_count++] = (uint8_t)c;
    }

    spin_unlock(kmsg_lock);
    return read_count;
}