#include "driver/serial.h"
#include "driver/ns16550.h"
#include "driver/tty.h"
#include "errno.h"
#include "lib/libfdt/libfdt.h"
#include "mem/frame.h"
#include "mem/page.h"

extern void             *opensbi_dtb_vaddr; // start.c
struct fdt_serial_device fdt_serial;
bool                     serial_initialized = false;
uart_device_t            uart0;
uart_config_t            config;

static const char *serial_compatibles[] = {
    "ns16550a", "snps,dw-apb-uart", "sifive,uart0", "riscv,uart0", "uart0", NULL,
};

static int find_serial_chosem(struct fdt_serial_device *device) {
    int chosen_off = fdt_path_offset(opensbi_dtb_vaddr, "/chosen");
    if (chosen_off < 0) return -ENODEV;
    int         len         = 0;
    const char *stdout_path = fdt_getprop(opensbi_dtb_vaddr, chosen_off, "stdout-path", &len);
    if (!stdout_path || len <= 0) return -ENODEV;
    char path_buf[128];

    const char *colon    = strchr(stdout_path, ':');
    size_t      path_len = colon ? (size_t)(colon - stdout_path) : strlen(stdout_path);
    if (path_len >= sizeof(path_buf)) return -ENAMETOOLONG;
    memcpy(path_buf, stdout_path, path_len);
    path_buf[path_len] = '\0';

    int serial_off = fdt_path_offset(opensbi_dtb_vaddr, path_buf);
    if (serial_off < 0) return -ENODEV;

    const uint64_t *reg = fdt_getprop(opensbi_dtb_vaddr, serial_off, "reg", &len);
    if (reg && len >= sizeof(uint64_t)) { device->base_addr = fdt64_to_cpu(*reg); }

    const uint32_t *irq = fdt_getprop(opensbi_dtb_vaddr, serial_off, "interrupts", &len);
    if (irq && len >= sizeof(uint32_t)) { fdt_serial.irq_num = fdt32_to_cpu(*irq); }

    const uint32_t *reg_shift = fdt_getprop(opensbi_dtb_vaddr, serial_off, "reg-shift", &len);
    fdt_serial.reg_shift      = reg_shift ? fdt32_to_cpu(*reg_shift) : 0;

    const uint32_t *reg_io_width = fdt_getprop(opensbi_dtb_vaddr, serial_off, "reg-io-width", &len);
    fdt_serial.reg_io_width      = reg_io_width ? fdt32_to_cpu(*reg_io_width) : 1;

    const uint32_t *clock_freq =
        fdt_getprop(opensbi_dtb_vaddr, serial_off, "clock-frequency", &len);
    fdt_serial.clock_freq = clock_freq ? fdt32_to_cpu(*clock_freq) : 0;

    fdt_serial.found = 1;

    return EOK;
}

int find_serial_by_compatible(struct fdt_serial_device *serial) {
    void *fdt = opensbi_dtb_vaddr;
    int   node;

    memset(serial, 0, sizeof(*serial));

    for (node = fdt_next_node(fdt, -1, NULL); node >= 0; node = fdt_next_node(fdt, node, NULL)) {

        const char *compatible = fdt_getprop(fdt, node, "compatible", NULL);
        if (!compatible) continue;

        // 匹配已知串口类型
        for (int i = 0; serial_compatibles[i]; i++) {
            if (strstr(compatible, serial_compatibles[i])) {
                int            len;
                const fdt64_t *reg = fdt_getprop(fdt, node, "reg", &len);
                if (reg && len >= sizeof(fdt64_t)) {
                    serial->base_addr = fdt64_to_cpu(reg[0]);
                    serial->found     = 1;

                    // interrupts
                    const fdt32_t *irq = fdt_getprop(fdt, node, "interrupts", &len);
                    if (irq && len >= sizeof(fdt32_t)) serial->irq_num = fdt32_to_cpu(irq[0]);

                    // reg-shift
                    const fdt32_t *reg_shift = fdt_getprop(fdt, node, "reg-shift", &len);
                    if (reg_shift && len >= sizeof(fdt32_t))
                        serial->reg_shift = fdt32_to_cpu(*reg_shift);

                    // reg-io-width
                    const fdt32_t *reg_io_width = fdt_getprop(fdt, node, "reg-io-width", &len);
                    if (reg_io_width && len >= sizeof(fdt32_t))
                        serial->reg_io_width = fdt32_to_cpu(*reg_io_width);

                    // clock-frequency
                    const fdt32_t *clock = fdt_getprop(fdt, node, "clock-frequency", &len);
                    if (clock && len >= sizeof(fdt32_t)) serial->clock_freq = fdt32_to_cpu(*clock);

                    return EOK;
                }
            }
        }
    }

    return -ENODEV;
}

char read_serial() {
    if (!serial_initialized) return 0;
    return uart_getc(&uart0);
}

void write_serial(char a) {
    if (!serial_initialized) return;
    uart_putc(&uart0, a);
}

static void serial_flush(tty_device_t *device) {}

static size_t serial_write(tty_device_t *device, const char *buf, size_t count) {
    for (size_t i = 0; i < count; i++) {
        write_serial(buf[i]);
    }
    return count;
}

static size_t serial_read(tty_device_t *device, char *buf, size_t count) {
    for (size_t i = 0; i < count; i++) {
        buf[i] = read_serial();
    }
    return count;
}

int init_serial() {
    if (find_serial_chosem(&fdt_serial) == EOK);
    else if (find_serial_by_compatible(&fdt_serial) == EOK);
    else return -ENODEV;

    serial_initialized = true;
    uint64_t virt = (uint64_t)phys_to_virt(fdt_serial.base_addr);
    page_map_range(get_kernel_pagedir(), virt, fdt_serial.base_addr, PAGE_SIZE,
                   ARCH_PT_FLAG_VALID | ARCH_PT_FLAG_READ | ARCH_PT_FLAG_WRITE |
                       ARCH_PT_FLAG_ACCESSED | ARCH_PT_FLAG_DIRTY);
    uart_init(&uart0, (volatile void *)virt, NULL);
    tty_device_t       *device = alloc_tty_device(TTY_DEVICE_SERIAL);
    struct tty_serial_ *data   = malloc(sizeof(struct tty_serial_));

    device->private_data = data;
    device->ops.read     = serial_read;
    device->ops.write    = serial_write;
    device->ops.flush    = serial_flush;

    char name[20];
    sprintf(name, "ttyS%d", 0);
    strcpy(device->name, name);
    register_tty_device(device);
    return EOK;
}
