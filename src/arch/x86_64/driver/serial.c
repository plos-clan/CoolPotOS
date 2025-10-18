#include "driver/serial.h"
#include "driver/tty.h"
#include "io.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "term/klog.h"

static bool     s_port[4]    = {false, false, false, false};
static uint16_t com_ports[4] = {SERIAL_PORT_1, SERIAL_PORT_2, SERIAL_PORT_3, SERIAL_PORT_4};

static size_t extract_number_serial(const char *str) {
    size_t len = strlen(str);

    if (len <= 4) { return -1; }

    size_t start_index = len;

    for (size_t i = len - 1; i >= 0; i--) {
        if (isdigit((unsigned char)str[i])) {
            start_index = i;
        } else {
            break;
        }

        if (i == 0) break;
    }
    if (start_index < 4 || start_index >= len) { return -1; }

    int         result = 0;
    const char *p      = str + start_index;

    while (isdigit((unsigned char)*p)) {
        int digit = *p - '0';
        result    = result * 10 + digit;
        p++;
    }

    return result;
}

static uint8_t serial_calculate_lcr(void) {
    uint8_t lcr = 0;

    switch (SERIAL_DATA_BITS) {
    case 5: lcr |= 0x00; break;
    case 6: lcr |= 0x01; break;
    case 7: lcr |= 0x02; break;
    case 8: lcr |= 0x03; break;
    default: lcr |= 0x03;
    }

    if (SERIAL_STOP_BITS == 2) lcr |= 0x04;

    switch (SERIAL_PARITY) {
    case 0: // No parity
        lcr |= 0x00;
        break;
    case 1: // Odd parity
        lcr |= 0x08;
        break;
    case 2: // Even parity
        lcr |= 0x18;
        break;
    case 3: // Mark parity
        lcr |= 0x28;
        break;
    case 4: // Space parity
        lcr |= 0x38;
        break;
    default: lcr |= 0x00;
    }
    return lcr;
}

static int serial_exists(uint16_t port) {
    uint8_t original_lcr = io_in8(port + SERIAL_REG_LCR);

    io_out8(port + SERIAL_REG_LCR, 0xaa);
    if (io_in8(port + SERIAL_REG_LCR) != 0xaa) {
        io_out8(port + SERIAL_REG_LCR, original_lcr);
        return 0;
    }

    io_out8(port + SERIAL_REG_LCR, 0x55);
    if (io_in8(port + SERIAL_REG_LCR) != 0x55) {
        io_out8(port + SERIAL_REG_LCR, original_lcr);
        return 0;
    }

    io_out8(port + SERIAL_REG_LCR, original_lcr);
    return 1;
}

static void init_serial_port(uint16_t port) {
    uint16_t divisor = 115200 / SERIAL_BAUD_RATE;

    io_out8(port + SERIAL_REG_IER, 0x00);                   // Disable COM interrupts
    io_out8(port + SERIAL_REG_LCR, 0x80);                   // Enable DLAB (set baud rate divisor)
    io_out8(port + SERIAL_REG_DATA, divisor & 0xff);        // Set low baud rate
    io_out8(port + SERIAL_REG_IER, (divisor >> 8) & 0xff);  // Set high baud rate
    io_out8(port + SERIAL_REG_LCR, serial_calculate_lcr()); // Set LCR
    io_out8(port + SERIAL_REG_FCR, 0xcf);                   // Enable FIFO with 14-byte threshold
    io_out8(port + SERIAL_REG_MCR, 0x0f);                   // Enable IRQ, set RTS/DSR
    io_out8(port + SERIAL_REG_MCR, 0x1e);  // Set to loopback mode and test the serial port
    io_out8(port + SERIAL_REG_DATA, 0xae); // Test serial port

    /* Check if there is a problem with the serial port */
    if (io_in8(port + SERIAL_REG_DATA) != 0xae) {
        logkf("serial: Serial port %s test failed.\n", PORT_TO_COM(port));
        return;
    }
    io_out8(port + SERIAL_REG_MCR, 0x0f); // Quit loopback mode
    logkf("serial: Local port: %s, Baud rate: %d, Status: 0x%02x\n", PORT_TO_COM(port),
          SERIAL_BAUD_RATE, io_in8(port + SERIAL_REG_LSR));
}

char read_serial(uint16_t port) {
    while ((io_in8(port + 5) & 1) == 0)
        ;
    return io_in8(port);
}

void write_serial(char a) {
    if (s_port[0]) write_serial0(SERIAL_PORT_1, a);
    if (s_port[1]) write_serial0(SERIAL_PORT_2, a);
    if (s_port[2]) write_serial0(SERIAL_PORT_3, a);
    if (s_port[3]) write_serial0(SERIAL_PORT_4, a);
}

void write_serial0(uint16_t port, char a) {
    while ((io_in8(port + 5) & 0x20) == 0)
        ;
    io_out8(port, a);
}

static void serial_flush(tty_device_t *device) {}

static size_t serial_write(tty_device_t *device, const char *buf, size_t count) {
    struct tty_serial_ *data = device->private_data;
    for (size_t i = 0; i < count; i++) {
        write_serial0(data->port, buf[i]);
    }
    return count;
}

static size_t serial_read(tty_device_t *device, char *buf, size_t count) {
    struct tty_serial_ *data = device->private_data;
    for (size_t i = 0; i < count; i++) {
        buf[i] = read_serial(data->port);
    }
    return count;
}

int init_serial() {
    int valid_ports = 0;
    for (int i = 0; i < 4; i++) {
        if (serial_exists(com_ports[i])) {
            s_port[i] = true;
            init_serial_port(com_ports[i]);
            valid_ports++;

            tty_device_t       *device = alloc_tty_device(TTY_DEVICE_SERIAL);
            struct tty_serial_ *data   = malloc(sizeof(struct tty_serial_));

            data->port           = com_ports[i];
            device->private_data = data;
            device->ops.flush    = serial_flush;
            device->ops.write    = serial_write;
            device->ops.read     = serial_read;

            char name[20];
            sprintf(name, "ttyS%d", i);
            strcpy(device->name, name);
            register_tty_device(device);
        }
    }
    if (valid_ports == 0) logkf("serial: No serial port available.\n");
    return 0;
}
