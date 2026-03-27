#include "driver/pty.h"
#include "driver/ioctl.h"
#include "errno.h"
#include "fs/devtmpfs.h"
#include "fs/vfs.h"
#include "krlibc.h"
#include "mem/heap.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"

static pty_pair_t *pty_table[PTY_MAX_COUNT] = { NULL };
static spin_t pty_table_lock;
static int pty_next_index = 0;

// ===== Ring Buffer Operations =====

void pty_ringbuf_init(pty_ringbuf_t *rb, size_t capacity) {
    rb->buf      = (char *)malloc(capacity);
    rb->capacity = capacity;
    rb->head     = 0;
    rb->tail     = 0;
    rb->count    = 0;
}

void pty_ringbuf_destroy(pty_ringbuf_t *rb) {
    if (rb->buf) {
        free(rb->buf);
        rb->buf = NULL;
    }
}

size_t pty_ringbuf_available(pty_ringbuf_t *rb) {
    return rb->count;
}

size_t pty_ringbuf_write(pty_ringbuf_t *rb, const void *src, size_t len) {
    if (!rb->buf || !src)
        return 0;

    size_t space    = rb->capacity - rb->count;
    size_t to_write = len < space ? len : space;

    const char *csrc = (const char *)src;
    for (size_t i = 0; i < to_write; i++) {
        rb->buf[rb->tail] = csrc[i];
        rb->tail          = (rb->tail + 1) % rb->capacity;
        rb->count++;
    }

    return to_write;
}

size_t pty_ringbuf_read(pty_ringbuf_t *rb, void *dst, size_t len) {
    if (!rb->buf || !dst)
        return 0;

    size_t to_read = len < rb->count ? len : rb->count;

    char *cdst = (char *)dst;
    for (size_t i = 0; i < to_read; i++) {
        cdst[i]  = rb->buf[rb->head];
        rb->head = (rb->head + 1) % rb->capacity;
        rb->count--;
    }

    return to_read;
}

// ===== PTY Pair Management =====

void pty_init() {
    pty_table_lock = false;
    memset(pty_table, 0, sizeof(pty_table));
}

pty_pair_t *pty_alloc() {
    pty_pair_t *pair = (pty_pair_t *)calloc(1, sizeof(pty_pair_t));
    if (!pair)
        return NULL;

    spin_lock(pty_table_lock);

    // Find available index
    int index = -1;
    for (int i = 0; i < PTY_MAX_COUNT; i++) {
        int check_idx = (pty_next_index + i) % PTY_MAX_COUNT;
        if (pty_table[check_idx] == NULL) {
            index            = check_idx;
            pty_table[index] = pair;
            pty_next_index   = (index + 1) % PTY_MAX_COUNT;
            break;
        }
    }

    spin_unlock(pty_table_lock);

    if (index == -1) {
        free(pair);
        return NULL;
    }

    pair->index       = index;
    pair->locked      = 0;
    pair->master_open = 0;
    pair->slave_open  = 0;
    pair->refcount    = 0;
    pair->lock        = false;

    pty_ringbuf_init(&pair->master_to_slave, PTY_BUF_SIZE);
    pty_ringbuf_init(&pair->slave_to_master, PTY_BUF_SIZE);

    pair->master_node = NULL;
    pair->slave_node  = NULL;

    // Default termios (sane defaults matching Linux)
    pair->termios.c_iflag = ICRNL | IXON;
    pair->termios.c_oflag = OPOST | ONLCR;
    pair->termios.c_cflag = CS8 | CREAD | HUPCL;
    pair->termios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
    pair->termios.c_line  = 0;
    memset(pair->termios.c_cc, 0, sizeof(pair->termios.c_cc));
    pair->termios.c_cc[VINTR]    = 003; // ^C
    pair->termios.c_cc[VQUIT]    = 034; // ^\
    pair->termios.c_cc[VERASE]   = 0177; // DEL
    pair->termios.c_cc[VKILL]    = 025; // ^U
    pair->termios.c_cc[VEOF]     = 004; // ^D
    pair->termios.c_cc[VSTART]   = 021; // ^Q
    pair->termios.c_cc[VSTOP]    = 023; // ^S
    pair->termios.c_cc[VSUSP]    = 032; // ^Z
    pair->termios.c_cc[VREPRINT] = 022; // ^R
    pair->termios.c_cc[VDISCARD] = 017; // ^O
    pair->termios.c_cc[VWERASE]  = 027; // ^W
    pair->termios.c_cc[VLNEXT]   = 026; // ^V
    pair->termios.c_cc[VMIN]     = 1;

    // Default window size
    pair->winsize.ws_row    = 24;
    pair->winsize.ws_col    = 80;
    pair->winsize.ws_xpixel = 0;
    pair->winsize.ws_ypixel = 0;

    pair->foreground_pgid = 0;
    pair->session_id      = 0;

    return pair;
}

void pty_free(pty_pair_t *pair) {
    if (!pair)
        return;

    spin_lock(pty_table_lock);
    if (pair->index >= 0 && pair->index < PTY_MAX_COUNT) {
        pty_table[pair->index] = NULL;
    }
    spin_unlock(pty_table_lock);

    pty_ringbuf_destroy(&pair->master_to_slave);
    pty_ringbuf_destroy(&pair->slave_to_master);
    free(pair);
}

pty_pair_t *pty_get(int index) {
    if (index < 0 || index >= PTY_MAX_COUNT)
        return NULL;

    spin_lock(pty_table_lock);
    pty_pair_t *pair = pty_table[index];
    spin_unlock(pty_table_lock);

    return pair;
}

// ===== VFS Operations for PTY Master =====

static size_t ptmx_read(void *file, void *addr, size_t offset, size_t size) {
    pty_specific_t *spec = (pty_specific_t *)file;
    if (!spec || !spec->active || !spec->pair)
        return -EBADF;

    pty_pair_t *pair = spec->pair;

    for (;;) {
        spin_lock(pair->lock);

        if (pair->master_open <= 0) {
            spin_unlock(pair->lock);
            return -EIO;
        }

        // Master reads from slave_to_master buffer
        size_t nread = pty_ringbuf_read(&pair->slave_to_master, addr, size);
        if (nread > 0) {
            spin_unlock(pair->lock);
            return nread;
        }

        // No data; if slave is closed, return EOF
        if (pair->slave_open <= 0) {
            spin_unlock(pair->lock);
            return 0;
        }

        // Block and wait for data
        spin_unlock(pair->lock);

        scheduler_yield();
    }
}

static size_t ptmx_write(void *file, const void *addr, size_t offset, size_t size) {
    pty_specific_t *spec = (pty_specific_t *)file;
    if (!spec || !spec->active || !spec->pair)
        return -EBADF;

    pty_pair_t *pair = spec->pair;

    spin_lock(pair->lock);

    if (pair->master_open <= 0) {
        spin_unlock(pair->lock);
        return -EIO;
    }

    const char *input = addr;
    size_t nwritten   = 0;

    for (size_t i = 0; i < size; i++) {
        char c = input[i];

        // Input processing (ICRNL: convert CR to NL)
        if (pair->termios.c_iflag & ICRNL && c == '\r') {
            c = '\n';
        }

        // Write to master_to_slave buffer (for the slave/shell to read)
        if (pty_ringbuf_write(&pair->master_to_slave, &c, 1) == 0) {
            break;
        }
        nwritten++;

        // Line discipline: ECHO - echo characters back to master (slave_to_master)
        if (pair->termios.c_lflag & ECHO) {
            if (c == '\n') {
                // Echo newline: if ONLCR is set, echo CR+LF
                if (pair->termios.c_oflag & ONLCR) {
                    pty_ringbuf_write(&pair->slave_to_master, "\r\n", 2);
                } else {
                    pty_ringbuf_write(&pair->slave_to_master, "\n", 1);
                }
            } else if (c == pair->termios.c_cc[VERASE]) {
                // Echo backspace: erase character on screen
                if (pair->termios.c_lflag & ECHOE) {
                    pty_ringbuf_write(&pair->slave_to_master, "\b \b", 3);
                }
            } else if ((unsigned char)c < 0x20 && c != '\t') {
                // Echo control characters as ^X if ECHOCTL is set
                if (pair->termios.c_lflag & ECHOCTL) {
                    char ctrl[2] = { '^', c + '@' };
                    pty_ringbuf_write(&pair->slave_to_master, ctrl, 2);
                }
            } else {
                // Echo normal character
                pty_ringbuf_write(&pair->slave_to_master, &c, 1);
            }
        }
    }

    spin_unlock(pair->lock);

    return nwritten;
}

static bool ptmx_close(void *file) {
    pty_specific_t *spec = (pty_specific_t *)file;
    if (!spec)
        return true;

    spec->active = false;

    if (spec->pair) {
        pty_pair_t *pair = spec->pair;

        spin_lock(pair->lock);
        if (spec->is_master) {
            pair->master_open--;
        } else {
            pair->slave_open--;
        }
        pair->refcount--;
        int refcount = pair->refcount;
        spin_unlock(pair->lock);

        // Free the pair if both sides are closed
        if (refcount <= 0 && pair->master_open <= 0 && pair->slave_open <= 0) {
            pty_free(pair);
        }
    }

    free(spec);
    return true;
}

static errno_t pts_ioctl(void *file, size_t req, void *arg);

static errno_t ptmx_ioctl(void *file, size_t req, void *arg) {
    pty_specific_t *spec = (pty_specific_t *)file;
    if (!spec || !spec->active || !spec->pair) {
        return -EBADF;
    }

    pty_pair_t *pair = spec->pair;

    switch (req) {
    case TIOCGPTN: {
        // Get PTY number
        int *ptn = (int *)arg;
        if (!ptn)
            return -EINVAL;
        *ptn = pair->index;
        return EOK;
    }
    case TIOCSPTLCK: {
        // Lock/unlock PTY
        int *lock = (int *)arg;
        if (!lock)
            return -EINVAL;
        spin_lock(pair->lock);
        pair->locked = *lock;
        spin_unlock(pair->lock);
        return EOK;
    }
    case TIOCGPTLCK: {
        // Get lock state
        int *lock = (int *)arg;
        if (!lock)
            return -EINVAL;
        spin_lock(pair->lock);
        *lock = pair->locked;
        spin_unlock(pair->lock);
        return EOK;
    }
    default:
        return pts_ioctl(file, req, arg);
    }
}

static errno_t ptmx_poll(void *file, size_t events) {
    pty_specific_t *spec = (pty_specific_t *)file;
    if (!spec || !spec->active || !spec->pair)
        return 0;

    pty_pair_t *pair = spec->pair;
    size_t revents   = 0;

    spin_lock(pair->lock);

    if (spec->is_master) {
        // Master side
        if ((events & EPOLLIN) && pty_ringbuf_available(&pair->slave_to_master) > 0) {
            revents |= EPOLLIN;
        }
        if (events & EPOLLOUT) {
            // Can always write (or check space in master_to_slave)
            revents |= EPOLLOUT;
        }
    } else {
        // Slave side
        if ((events & EPOLLIN) && pty_ringbuf_available(&pair->master_to_slave) > 0) {
            revents |= EPOLLIN;
        }
        if (events & EPOLLOUT) {
            revents |= EPOLLOUT;
        }
    }

    spin_unlock(pair->lock);

    return revents;
}

static size_t pts_read(void *file, void *addr, size_t offset, size_t size) {
    pty_specific_t *spec = (pty_specific_t *)file;
    if (!spec || !spec->active || !spec->pair)
        return -EBADF;

    pty_pair_t *pair = spec->pair;

    for (;;) {
        spin_lock(pair->lock);

        if (pair->slave_open <= 0) {
            spin_unlock(pair->lock);
            return -EIO;
        }

        // Slave reads from master_to_slave buffer
        size_t nread = pty_ringbuf_read(&pair->master_to_slave, addr, size);
        if (nread > 0) {
            spin_unlock(pair->lock);
            return nread;
        }

        // No data; if master is closed, return EOF
        if (pair->master_open <= 0) {
            spin_unlock(pair->lock);
            return 0;
        }

        // Block and wait for data
        spin_unlock(pair->lock);
        scheduler_yield();
    }
}

static size_t pts_write(void *file, const void *addr, size_t offset, size_t size) {
    pty_specific_t *spec = (pty_specific_t *)file;
    if (!spec || !spec->active || !spec->pair)
        return -EBADF;

    pty_pair_t *pair = spec->pair;

    spin_lock(pair->lock);

    if (pair->slave_open <= 0) {
        spin_unlock(pair->lock);
        return -EIO;
    }

    // Output processing: OPOST + ONLCR (map NL to CR-NL on output)
    if ((pair->termios.c_oflag & OPOST) && (pair->termios.c_oflag & ONLCR)) {
        const char *input = (const char *)addr;
        size_t nwritten   = 0;
        for (size_t i = 0; i < size; i++) {
            if (input[i] == '\n') {
                if (pty_ringbuf_write(&pair->slave_to_master, "\r\n", 2) < 2)
                    break;
            } else {
                if (pty_ringbuf_write(&pair->slave_to_master, &input[i], 1) == 0)
                    break;
            }
            nwritten++;
        }
        spin_unlock(pair->lock);
        return nwritten;
    }

    // Slave writes to slave_to_master buffer
    size_t nwritten = pty_ringbuf_write(&pair->slave_to_master, addr, size);

    spin_unlock(pair->lock);

    return nwritten;
}

static errno_t pts_ioctl(void *file, size_t req, void *arg) {
    pty_specific_t *spec = (pty_specific_t *)file;
    if (!spec || !spec->active || !spec->pair)
        return -EBADF;

    pty_pair_t *pair = spec->pair;

    switch (req) {
    case TCGETS:
        if (!arg)
            return -EINVAL;
        memcpy(arg, &pair->termios, sizeof(termios_t));
        return EOK;
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
        if (!arg)
            return -EINVAL;
        memcpy(&pair->termios, arg, sizeof(termios_t));
        return EOK;
    case TIOCGWINSZ:
        if (!arg)
            return -EINVAL;
        memcpy(arg, &pair->winsize, sizeof(struct winsize));
        return EOK;
    case TIOCSWINSZ:
        if (!arg)
            return -EINVAL;
        memcpy(&pair->winsize, arg, sizeof(struct winsize));
        return EOK;
    case TIOCSCTTY: {
        tcb_t thread          = get_current_task();
        pcb_t proc            = thread->process;
        pair->session_id      = proc->sid;
        pair->foreground_pgid = proc->pgid;
        if (proc->ctty_path)
            free(proc->ctty_path);
        char buf[64];
        snprintf(buf, sizeof(buf), "/dev/pts/%d", pair->index);
        proc->ctty_path = strdup(buf);
        return EOK;
    }
    case TIOCGPGRP:
        if (!arg)
            return -EINVAL;
        *(pid_t *)arg = pair->foreground_pgid;
        return EOK;
    case TIOCSPGRP:
        if (!arg)
            return -EINVAL;
        pair->foreground_pgid = *(pid_t *)arg;
        return EOK;
    case TIOCGSID:
        if (!arg)
            return -EINVAL;
        *(pid_t *)arg = pair->session_id;
        return EOK;
    case TIOCGPTN:
        if (!arg)
            return -EINVAL;
        *(int *)arg = pair->index;
        return EOK;
    case TIOCSPTLCK:
        if (!arg)
            return -EINVAL;
        pair->locked = *(int *)arg;
        return EOK;
    case TIOCGPTLCK:
        if (!arg)
            return -EINVAL;
        *(int *)arg = pair->locked;
        return EOK;
    case FIONREAD:
        if (!arg)
            return -EINVAL;
        if (spec->is_master)
            *(int *)arg = (int)pty_ringbuf_available(&pair->slave_to_master);
        else
            *(int *)arg = (int)pty_ringbuf_available(&pair->master_to_slave);
        return EOK;
    default:
        return -ENOTTY;
    }
}

// ===== /dev/ptmx Device Operations =====

static size_t ptmx_device_read(void *handle, void *addr, size_t offset, size_t size) {
    return ptmx_read(handle, addr, offset, size);
}

static size_t ptmx_device_write(void *handle, const void *addr, size_t offset, size_t size) {
    return ptmx_write(handle, addr, offset, size);
}

static errno_t ptmx_device_ioctl(void *handle, size_t req, void *arg) {
    return ptmx_ioctl(handle, req, arg);
}

static errno_t pts_device_ioctl(void *handle, size_t req, void *arg) {
    return pts_ioctl(handle, req, arg);
}

static errno_t ptmx_device_poll(void *handle, size_t events) {
    return ptmx_poll(handle, events);
}

static size_t pts_device_read(void *handle, void *addr, size_t offset, size_t size) {
    return pts_read(handle, addr, offset, size);
}

static size_t pts_device_write(void *handle, const void *addr, size_t offset, size_t size) {
    return pts_write(handle, addr, offset, size);
}

static size_t ptmx_size_func(void *handle) {
    return 0;
}

// Forward declaration
static void pts_device_open(void *parent, const char *name, vfs_node_t node);
static bool ptmx_device_close(void *file);

// Called when /dev/ptmx is opened
static void ptmx_device_open(void *parent, const char *name, vfs_node_t node) {
    // Allocate a new PTY pair
    pty_pair_t *pair = pty_alloc();
    if (!pair) {
        return;
    }

    // Create master-side specific data
    pty_specific_t *spec = (pty_specific_t *)calloc(1, sizeof(pty_specific_t));
    if (!spec) {
        pty_free(pair);
        return;
    }

    spec->pair      = pair;
    spec->is_master = true;
    spec->active    = true;

    spin_lock(pair->lock);
    pair->master_open++;
    pair->master_node = node;
    pair->refcount++;
    spin_unlock(pair->lock);

    node->type = file_ptmx;

    // Update the device_handle in the dtmp_handle_t structure
    // This is what devtmpfs passes to our ioctl/read/write functions
    dtmp_handle_t *dtmp = (dtmp_handle_t *)node->handle;
    if (dtmp && dtmp->type == dtp_file_device) {
        dtmp->device_handle = spec;
    }

    // Create the corresponding /dev/pts/<index> device node
    char pts_name[32];
    snprintf(pts_name, sizeof(pts_name), "%d", pair->index);

    vfs_node_t pts_root = vfs_open("/dev/pts");
    if (pts_root) {
        // Create pts device with open_t callback
        errno_t err = create_device_node_ex(
            pts_root,
            pts_name,
            device_stream,
            NULL,
            0,
            pts_device_open, // Pass open_t callback!
            ptmx_device_close,
            pts_device_ioctl,
            pts_device_read,
            pts_device_write,
            ptmx_device_poll,
            NULL,
            ptmx_size_func
        );
        vfs_close(pts_root);

        if (err == EOK) {
            char pts_path[64];
            snprintf(pts_path, sizeof(pts_path), "/dev/pts/%d", pair->index);
            vfs_node_t pts_node = vfs_open(pts_path);
            if (pts_node) {
                // Store pair index for later lookup
                pts_node->rdev = pair->index;
                vfs_update(pts_node);
                vfs_close(pts_node);
            }
        }
    }
}

static bool ptmx_device_close(void *file) {
    return ptmx_close(file);
}

static void pts_device_open(void *parent, const char *name, vfs_node_t node) {
    // Parse PTY index from name
    int index = 0;
    for (const char *p = name; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            index = index * 10 + (*p - '0');
        }
    }

    // Get PTY pair
    pty_pair_t *pair = pty_get(index);
    if (!pair) {
        return;
    }

    // Check if locked
    spin_lock(pair->lock);
    int locked = pair->locked;
    spin_unlock(pair->lock);

    if (locked) {
        return;
    }

    // Create slave-side specific data
    pty_specific_t *spec = (pty_specific_t *)calloc(1, sizeof(pty_specific_t));
    if (!spec) {
        return;
    }

    spec->pair      = pair;
    spec->is_master = false;
    spec->active    = true;

    spin_lock(pair->lock);
    pair->slave_open++;
    pair->slave_node = node;
    pair->refcount++;
    spin_unlock(pair->lock);

    node->type = file_pts;

    // Update the device_handle in the dtmp_handle_t structure
    dtmp_handle_t *dtmp = (dtmp_handle_t *)node->handle;
    if (dtmp && dtmp->type == dtp_file_device) {
        dtmp->device_handle = spec;
    }
}

// ===== Initialization =====

void ptmx_init() {
    pty_init();

    // Create /dev/ptmx device with open_t callback
    vfs_node_t dev_root = vfs_open("/dev");
    if (!dev_root) {
        logkf("ptmx_init: /dev not found\n");
        return;
    }

    errno_t err = create_device_node_ex(
        dev_root,
        "ptmx",
        device_stream,
        NULL,
        0,
        ptmx_device_open, // Pass open_t here!
        ptmx_device_close,
        ptmx_device_ioctl,
        ptmx_device_read,
        ptmx_device_write,
        ptmx_device_poll,
        NULL,
        ptmx_size_func
    );

    vfs_close(dev_root);

    if (err != EOK) {
        logkf("ptmx_init: failed to create /dev/ptmx: %d\n", err);
        return;
    }

    // Create /dev/pts directory
    vfs_mkdir("/dev/pts");

    logkf("ptmx: initialized\n");
}
