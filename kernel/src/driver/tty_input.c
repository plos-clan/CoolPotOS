#include "driver/tty.h"
#include "krlibc.h"
#include "lock.h"
#include "term/kprint.h"

/*
 * TTY 输入缓冲区 - 简单的环形缓冲区
 * 用于键盘输入处理
 */
#define TTY_INPUT_BUFFER_SIZE 256

typedef struct tty_input_buffer {
    char    data[TTY_INPUT_BUFFER_SIZE];
    size_t  head;
    size_t  tail;
    size_t  count;
    spin_t  lock;
} tty_input_buffer_t;

static tty_input_buffer_t g_input_buffer;

/* 初始化输入缓冲区 */
static void tty_input_init(void) {
    memset(&g_input_buffer, 0, sizeof(g_input_buffer));
    g_input_buffer.lock = SPIN_INIT;
}

/* 向输入缓冲区写入一个字符 */
int tty_input_putchar(char ch) {
    spin_lock(g_input_buffer.lock);
    if (g_input_buffer.count >= TTY_INPUT_BUFFER_SIZE) {
        spin_unlock(g_input_buffer.lock);
        return -ENOSPC;
    }
    g_input_buffer.data[g_input_buffer.head] = ch;
    g_input_buffer.head = (g_input_buffer.head + 1) % TTY_INPUT_BUFFER_SIZE;
    g_input_buffer.count++;
    spin_unlock(g_input_buffer.lock);
    return 0;
}

/* 从输入缓冲区读取一个字符 */
int tty_input_getchar(void) {
    spin_lock(g_input_buffer.lock);
    if (g_input_buffer.count == 0) {
        spin_unlock(g_input_buffer.lock);
        return -1;
    }
    char ch = g_input_buffer.data[g_input_buffer.tail];
    g_input_buffer.tail = (g_input_buffer.tail + 1) % TTY_INPUT_BUFFER_SIZE;
    g_input_buffer.count--;
    spin_unlock(g_input_buffer.lock);
    return (int)(unsigned char)ch;
}

/* 从输入缓冲区读取一行 (直到换行) */
ssize_t tty_input_readline(char *buf, size_t size) {
    if (!buf || size == 0) return -EINVAL;
    size_t i = 0;
    while (i < size - 1) {
        int ch = tty_input_getchar();
        if (ch < 0) break;
        buf[i++] = (char)ch;
        if (ch == '\n') break;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

/* 检查输入缓冲区是否为空 */
bool tty_input_empty(void) {
    spin_lock(g_input_buffer.lock);
    bool empty = (g_input_buffer.count == 0);
    spin_unlock(g_input_buffer.lock);
    return empty;
}

/* 清空输入缓冲区 */
void tty_input_flush(tty_t *session) {
    (void)session;
    spin_lock(g_input_buffer.lock);
    g_input_buffer.head  = 0;
    g_input_buffer.tail  = 0;
    g_input_buffer.count = 0;
    spin_unlock(g_input_buffer.lock);
}

/* TTY 会话读取操作 */
ssize_t tty_session_read(tty_t *session, char *buf, size_t count) {
    return tty_input_readline(buf, count);
}

/* TTY 会话轮询操作 */
int tty_session_poll(tty_t *session, int events) {
    int revents = 0;
    if ((events & POLLIN) && !tty_input_empty()) {
        revents |= POLLIN;
    }
    if (events & POLLOUT) {
        revents |= POLLOUT;
    }
    return revents;
}

/* 键盘中断处理 - 简单扫描码到 ASCII 转换 */
void tty_handle_keyboard(uint8_t scancode) {
    /* 简单的 US QWERTY 键盘扫描码集1 到 ASCII 映射 */
    static const char scancode_to_ascii[] = {
        0,    0,    '1', '2', '3', '4', '5', '6', '7', '8',  /* 0x00-0x09 */
        '9',  '0', '-', '=', 0,   0,   'q', 'w', 'e', 'r',  /* 0x0A-0x13 */
        't',  'y', 'u', 'i', 'o', 'p', '[', ']', 0,   0,    /* 0x14-0x1D */
        'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 0x1E-0x27 */
        '\'', '`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',  /* 0x28-0x31 */
        'm',  ',', '.', '/', 0,   0,   0,   ' ', 0,   0,     /* 0x32-0x3B */
    };

    if (scancode < sizeof(scancode_to_ascii)) {
        char ch = scancode_to_ascii[scancode];
        if (ch != 0) {
            tty_input_putchar(ch);
            /* 回显 */
            const device_t *tty = device_find(DEV_TTY, 0);
            if (tty) {
                device_write(tty->dev, &ch, 0, 1);
            }
        }
    }

    /* 特殊按键处理 */
    if (scancode == 0x1C) { /* Enter */
        tty_input_putchar('\n');
        const device_t *tty = device_find(DEV_TTY, 0);
        if (tty) {
            char nl = '\n';
            device_write(tty->dev, &nl, 0, 1);
        }
    } else if (scancode == 0x0E) { /* Backspace */
        spin_lock(g_input_buffer.lock);
        if (g_input_buffer.count > 0) {
            /* 从缓冲区移除最后一个字符 */
            g_input_buffer.head = (g_input_buffer.head > 0) ?
                g_input_buffer.head - 1 : TTY_INPUT_BUFFER_SIZE - 1;
            g_input_buffer.count--;
            spin_unlock(g_input_buffer.lock);
            /* 回显退格 */
            const device_t *tty = device_find(DEV_TTY, 0);
            if (tty) {
                char bs = '\b';
                device_write(tty->dev, &bs, 0, 1);
            }
        } else {
            spin_unlock(g_input_buffer.lock);
        }
    }
}

/* 初始化 TTY 输入子系统 */
void tty_input_init_subsystem(void) {
    tty_input_init();
    kdebug("TTY input subsystem initialized");
}