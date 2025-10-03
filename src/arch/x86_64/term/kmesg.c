#include "kmesg.h"
#include "device.h"
#include "errno.h"
#include "krlibc.h"
#include "poll.h"

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

static size_t kmesg_write(int drive, uint8_t *buffer, size_t number, size_t lba) {
    UNUSED(drive, buffer, number, lba);
    // 只读设备，返回错误
    return 0;
}

static int kmesg_ioctl(struct _device *device, size_t req, void *handle) {
    (void)device;
    switch (req) {
    case 0x01: // 清空缓冲区
        kmsg_head = kmsg_tail = 0;
        return 0;
    case 0x02: // 获取当前长度
        *(size_t *)handle = (kmsg_head >= kmsg_tail) ? (kmsg_head - kmsg_tail)
                                                     : (KMSG_SIZE - (kmsg_tail - kmsg_head));
        return 0;
    default: return -ENOTTY; // 不支持的 ioctl
    }
}

static int kmesg_poll(size_t events) {
    if ((events & POLLIN) && !kmsg_empty()) return POLLIN;
    return 0;
}

void build_kmesg_device() {
    device_t kmesg;
    kmesg.type = DEVICE_STREAM;
    strcpy(kmesg.drive_name, "kmesg");
    kmesg.flag        = 1;
    kmesg.sector_size = 1;
    kmesg.size        = 1;
    kmesg.read        = kmesg_read;
    kmesg.write       = kmesg_write;
    kmesg.ioctl       = kmesg_ioctl;
    kmesg.poll        = kmesg_poll;
    kmesg.map         = (void *)empty;
    regist_device(NULL, kmesg);
}
