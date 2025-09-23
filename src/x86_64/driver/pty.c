#define ALL_IMPLEMENTATION

#include "pty.h"
#include "id_alloc.h"
#include "ioctl.h"
#include "kprint.h"
#include "krlibc.h"
#include "list.h"
#include "pcb.h"
#include "scheduler.h"
#include "sprintf.h"

static int           ptmx_fsid = 0;
static int           pts_fsid  = 0;
struct llist_header *ptmx_list_head;
spin_t               pty_global_lock = SPIN_INIT;

static int dummy() {
    return -ENOSYS;
}

int str_to_int(const char *str, int *result) {
    int  sign  = 1;
    long value = 0;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    for (; *str != '\0'; str++) {
        if (!(*str >= '0' && *str <= '9')) { return -EINVAL; }

        value = value * 10 + (*str - '0');
    }

    value *= sign;

    *result = (int)value;
    return 0;
}

int pty_id_decide() {
    spin_lock(pty_global_lock);
    int ret = id_alloc();
    spin_unlock(pty_global_lock);
    return ret;
}

void pty_id_remove(int index) {
    spin_lock(pty_global_lock);
    id_free(index);
    spin_unlock(pty_global_lock);
}

void pty_termios_default(struct termios *term) {
    term->c_iflag = ICRNL | IXON | BRKINT | ISTRIP | INPCK;
    term->c_oflag = OPOST | ONLCR;
    term->c_cflag = B38400 | CS8 | CREAD | HUPCL;
    term->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;

    term->c_cc[VINTR]  = 3;   // Ctrl-C
    term->c_cc[VQUIT]  = 28;  // Ctrl-backslash
    term->c_cc[VERASE] = 127; // DEL
    term->c_cc[VKILL]  = 21;  // Ctrl-U
    term->c_cc[VEOF]   = 4;   // Ctrl-D
    term->c_cc[VTIME]  = 0;
    term->c_cc[VMIN]   = 1;
    term->c_cc[VSTART] = 17; // Ctrl-Q
    term->c_cc[VSTOP]  = 19; // Ctrl-S
    term->c_cc[VSUSP]  = 26; // Ctrl-Z
}

void pty_pair_cleanup(pty_handle_t *pair) {
    free(pair->master_buffer);
    free(pair->slave_buffer);
    pty_id_remove(pair->id);
    llist_delete(&pair->list_node);
}

size_t ptmx_data_avail(pty_handle_t *pair) {
    return pair->ptr_master; // won't matter here
}

void pts_ctrl_assign(pty_handle_t *pair) {
    // currentTask->ctrlPty = pair->id;
    // pair->ctrl_session = current_task->sid;
    pair->ctrl_pgid = get_current_task()->parent_group->pgid;
}

size_t pts_data_avali(pty_handle_t *pair) {
    bool canonical = pair->term.c_lflag & ICANON;
    if (!canonical) return pair->ptr_slave; // flush whatever we can

    // now we're on canonical mode
    for (size_t i = 0; i < pair->ptr_slave; i++) {
        if (pair->slave_buffer[i] == '\n' || pair->slave_buffer[i] == pair->term.c_cc[VEOF] ||
            pair->slave_buffer[i] == pair->term.c_cc[VEOL] ||
            pair->slave_buffer[i] == pair->term.c_cc[VEOL2])
            return i + 1; // +1 for len
    }
    return 0; // nothing found
}

void ptmx_device_open(void *file, const char *name, vfs_node_t node) {
    pty_handle_t *handle = calloc(1, sizeof(pty_handle_t));
    llist_init_head(&handle->list_node);
    pty_termios_default(&handle->term);
    handle->win.ws_row    = 24;
    handle->win.ws_col    = 80;
    handle->master_fds    = 1;
    handle->tty_kbmode    = K_XLATE;
    handle->master_buffer = calloc(1, PTY_BUFF_SIZE);
    handle->slave_buffer  = calloc(1, PTY_BUFF_SIZE);
    handle->id            = pty_id_decide();
    llist_append(ptmx_list_head, &handle->list_node);

    memset(&handle->vt_mode, 0, sizeof(struct vt_mode));
    handle->node   = node;
    node->handle   = handle;
    node->fsid     = ptmx_fsid;
    node->refcount = 1;

    vfs_node_t dev_root = node->parent;
    list_delete(dev_root->child, node);
    node->parent        = NULL;
    vfs_node_t new_node = vfs_node_alloc(dev_root, "ptmx");
    new_node->fsid      = ptmx_fsid;
    new_node->handle    = NULL;

    vfs_node_t pts_node = vfs_open("/dev/pts");
    pts_node->fsid      = pts_fsid;
    char nm[4];
    sprintf(nm, "%d", handle->id);
    vfs_node_t pty_slave_node = vfs_node_alloc(pts_node, nm);
    pty_slave_node->fsid      = pts_fsid;
}

void ptmx_device_close(void *handle0) {
    pty_handle_t *handle = handle0;
    spin_lock(handle->lock);
    handle->master_fds--;
    if (!handle->master_fds && !handle->slave_fds)
        pty_pair_cleanup(handle);
    else
        spin_unlock(handle->lock);

    free(handle->node->name);
    free(handle->node);
    free(handle);
}

size_t ptmx_device_read(void *file, void *addr, size_t offset, size_t size) {
    pty_handle_t *pair = file;
    while (true) {
        spin_lock(pair->lock);
        if (ptmx_data_avail(pair) > 0) {
            spin_unlock(pair->lock);
            break;
        }
        if (!pair->slave_fds) {
            spin_unlock(pair->lock);
            return 0;
        }

        spin_unlock(pair->lock);
        scheduler_yield();
    }
    spin_lock(pair->lock);

    close_interrupt;

    size_t toCopy = MIN(size, ptmx_data_avail(pair));
    memcpy(addr, pair->master_buffer, toCopy);
    memmove(pair->master_buffer, &pair->master_buffer[toCopy], PTY_BUFF_SIZE - toCopy);
    pair->ptr_master -= toCopy;

    spin_unlock(pair->lock);
    return toCopy;
}

size_t ptmx_device_write(void *file, const void *addr, size_t offset, size_t limit) {
    pty_handle_t *pair = file;
    while (true) {
        spin_lock(pair->lock);

        // if (!pair->slave_fds) {
        //     spin_unlock(pair->lock);
        //     return -ENXIO;
        // }
        if ((pair->ptr_slave + limit) < PTY_BUFF_SIZE) {
            spin_unlock(pair->lock);
            break;
        }

        spin_unlock(pair->lock);
        scheduler_yield();
    }

    spin_lock(pair->lock);

    memcpy(&pair->slave_buffer[pair->ptr_slave], addr, limit);
    if (pair->term.c_iflag & ICRNL)
        for (size_t i = 0; i < limit; i++) {
            if (pair->slave_buffer[pair->ptr_slave + i] == '\r')
                pair->slave_buffer[pair->ptr_slave + i] = '\n';
        }

    pair->ptr_slave += limit;

    spin_unlock(pair->lock);
    return limit;
}

errno_t ptmx_device_ioctl(void *handle, size_t request, void *arg) {
    pty_handle_t *pair   = handle;
    size_t        ret    = 0; // todo ERR(ENOTTY)
    size_t        number = _IOC_NR(request);

    spin_lock(pair->lock);
    switch (number) {
    case 0x31: { // TIOCSPTLCK
        int lock = *((int *)arg);
        if (lock == 0)
            pair->locked = false;
        else
            pair->locked = true;
        ret = 0;
        goto done;
    }
    case 0x30: // TIOCGPTN
        *((int *)arg) = pair->id;
        ret           = 0;
        goto done;
    }
    switch (request & 0xFFFFFFFF) {
    case TIOCGWINSZ: {
        memcpy((void *)arg, &pair->win, sizeof(struct winsize));
        ret = 0;
        break;
    }
    case TIOCSWINSZ: {
        memcpy(&pair->win, (const void *)arg, sizeof(struct winsize));
        ret = 0;
        break;
    }
    default: printk("ptmx_ioctl: Unsupported request %#010lx\n", request & 0xFFFFFFFF); break;
    }
done:
    spin_unlock(pair->lock);

    return ret;
}

errno_t ptmx_device_poll(void *file, size_t events) {
    pty_handle_t *pair    = file;
    int           revents = 0;
    spin_lock(pair->lock);
    if (ptmx_data_avail(pair) > 0 && events & EPOLLIN) revents |= EPOLLIN;
    if (pair->ptr_slave < PTY_BUFF_SIZE && events & EPOLLOUT) revents |= EPOLLOUT;
    spin_unlock(pair->lock);
    return revents;
}

vfs_node_t ptmx_device_dup(vfs_node_t src) {
    pty_handle_t *pair = src->handle;
    spin_lock(pair->lock);
    pair->master_fds++;
    spin_unlock(pair->lock);
    return src;
}

void pts_open(void *parent, const char *name, vfs_node_t node) {
    int length = strlen(name);

    int id;
    int res = str_to_int(name, &id);
    if (res < 0) return;
    spin_lock(pty_global_lock);
    pty_handle_t *pos, *n;
    pty_handle_t *browse = NULL;
    llist_for_each(pos, n, ptmx_list_head, list_node) {
        if (pos->id == id) {
            browse = pos;
            break;
        }
    }

    if (!browse) return;

    if (browse->locked) {
        spin_unlock(pty_global_lock);
        return;
    }

    node->handle = browse;
    node->type   = file_pts;
    node->fsid   = pts_fsid;
    browse->slave_fds++;

    spin_unlock(browse->lock);
}

void pts_close(void *fd) {
    pty_handle_t *pair = fd;
    spin_lock(pair->lock);
    pair->slave_fds--;
    if (!pair->master_fds && !pair->slave_fds)
        pty_pair_cleanup(pair);
    else
        spin_unlock(pair->lock);
}

size_t pts_read(void *file, uint8_t *out, size_t offset, size_t limit) {
    pty_handle_t *pair = file;
    while (true) {
        spin_lock(pair->lock);
        if (pts_data_avali(pair) > 0) {
            spin_unlock(pair->lock);
            break;
        }
        if (!pair->master_fds) {
            spin_unlock(pair->lock);
            return 0;
        }
        spin_unlock(pair->lock);
        scheduler_yield();
    }

    spin_lock(pair->lock);

    close_interrupt;

    size_t toCopy = MIN(limit, pts_data_avali(pair));
    memcpy(out, pair->slave_buffer, toCopy);
    memmove(pair->slave_buffer, &pair->slave_buffer[toCopy], PTY_BUFF_SIZE - toCopy);
    pair->ptr_slave -= toCopy;

    spin_unlock(pair->lock);
    return toCopy;
}

size_t pts_write_inner(pty_handle_t *pair, uint8_t *in, size_t limit) {
    size_t written     = 0;
    bool   doTranslate = (pair->term.c_oflag & OPOST) && (pair->term.c_oflag & ONLCR);
    for (size_t i = 0; i < limit; ++i) {
        uint8_t ch = in[i];
        if (doTranslate && ch == '\n') {
            if ((pair->ptr_master + 2) >= PTY_BUFF_SIZE) break;
            pair->master_buffer[pair->ptr_master++] = '\r';
            pair->master_buffer[pair->ptr_master++] = '\n';
            written++;
        } else {
            if ((pair->ptr_master + 1) >= PTY_BUFF_SIZE) break;
            pair->master_buffer[pair->ptr_master++] = ch;
            written++;
        }
    }
    return written;
}

size_t pts_write(void *file, uint8_t *in, size_t offset, size_t limit) {
    pty_handle_t *pair = file;

    while (true) {
        spin_lock(pair->lock);
        if (!pair->master_fds) {
            spin_unlock(pair->lock);
            // todo: send SIGHUP when master is closed, check controlling
            // term/group
            return VFS_STATUS_FAILED;
        }
        if ((pair->ptr_master + limit) < PTY_BUFF_SIZE) {
            spin_unlock(pair->lock);
            break;
        }
        spin_unlock(pair->lock);
        scheduler_yield();
    }

    spin_lock(pair->lock);
    size_t written = pts_write_inner(pair, in, limit);
    spin_unlock(pair->lock);
    return written;
}

size_t pts_ioctl(pty_handle_t *pair, uint64_t request, void *arg) {
    size_t ret = -ENOTTY;

    spin_lock(pair->lock);
    switch (request) {
    case TIOCGWINSZ: {
        memcpy(arg, &pair->win, sizeof(struct winsize));
        ret = 0;
        break;
    }
    case TIOCSWINSZ: {
        memcpy(&pair->win, arg, sizeof(struct winsize));
        ret = 0;
        break;
    }
    case TIOCSCTTY: {
        pts_ctrl_assign(pair);
        ret = 0;
        break;
    }
    case TCGETS: {
        memcpy(arg, &pair->term, sizeof(struct termios));
        ret = 0;
        break;
    }
    case TCSETS:
    case TCSETSW:   // this drains(?), idek man
    case TCSETSF: { // idek anymore man
        memcpy(&pair->term, arg, sizeof(struct termios));
        ret = 0;
        break;
    }
    case TIOCGPGRP:
        int *pid = (int *)arg;
        *pid     = get_current_task()->parent_group->pid;
        ret      = 0;
        break;
    case TIOCSPGRP: ret = 0; break;
    case KDGKBMODE: *(int *)arg = pair->tty_kbmode; return 0;
    case KDSKBMODE:
        pair->tty_kbmode = *(int *)arg;
        ret              = 0;
        break;
    case VT_SETMODE:
        memcpy(&pair->vt_mode, (void *)arg, sizeof(struct vt_mode));
        ret = 0;
        break;
    case VT_GETMODE:
        memcpy((void *)arg, &pair->vt_mode, sizeof(struct vt_mode));
        ret = 0;
        break;
    case VT_ACTIVATE: ret = 0; break;
    case VT_WAITACTIVE: ret = 0; break;
    case VT_GETSTATE:
        struct vt_state *state = (struct vt_state *)arg;
        state->v_active        = 2; // 当前活动终端
        state->v_state         = 0; // 状态标志
        ret                    = 0;
        break;
    case VT_OPENQRY:
        *(int *)arg = 2;
        ret         = 0;
        break;
    case TIOCNOTTY: ret = 0; break;
    default: printk("pts_ioctl: Unsupported request %#010lx\n", request); break;
    }
    spin_unlock(pair->lock);

    return ret;
}

int pts_poll(pty_handle_t *pair, int events) {
    int revents = 0;

    spin_lock(pair->lock);
    if ((!pair->master_fds || pts_data_avali(pair) > 0) && events & EPOLLIN) revents |= EPOLLIN;
    if (pair->ptr_master < PTY_BUFF_SIZE && events & EPOLLOUT) revents |= EPOLLOUT;
    spin_unlock(pair->lock);

    return revents;
}

vfs_node_t pts_dup(vfs_node_t node) {
    pty_handle_t *pair = node->handle;
    spin_lock(pair->lock);
    pair->slave_fds++;
    spin_unlock(pair->lock);
    return node;
}

static struct vfs_callback ptmx_callbacks = {
    .mount    = (vfs_mount_t)dummy,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = ptmx_device_open,
    .close    = ptmx_device_close,
    .read     = ptmx_device_read,
    .write    = ptmx_device_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .stat     = (vfs_stat_t)dummy,
    .ioctl    = ptmx_device_ioctl,
    .poll     = ptmx_device_poll,
    .dup      = ptmx_device_dup,
};

static struct vfs_callback pts_callbacks = {
    .mount    = (vfs_mount_t)dummy,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = (vfs_open_t)pts_open,
    .close    = (vfs_close_t)pts_close,
    .read     = (vfs_read_t)pts_read,
    .write    = (vfs_write_t)pts_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .rename   = (vfs_rename_t)dummy,
    .delete   = (vfs_del_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .stat     = (vfs_stat_t)dummy,
    .ioctl    = (vfs_ioctl_t)pts_ioctl,
    .poll     = (vfs_poll_t)pts_poll,
    .dup      = (vfs_dup_t)pts_dup,
};

static void setup_ptmx_device() {
    ptmx_fsid      = vfs_regist("ptyfs_master", &ptmx_callbacks);
    pts_fsid       = vfs_regist("ptyfs_slave", &pts_callbacks);
    ptmx_list_head = malloc(sizeof(struct llist_header));
    llist_init_head(ptmx_list_head);
    if (ptmx_fsid == VFS_STATUS_FAILED) {
        kerror("Cannot register virtual ptfs.");
        return;
    }

    vfs_node_t *devfs_root = vfs_open("/dev");
    if (devfs_root == NULL) {
        kerror("Cannot open devfs root directory.");
        return;
    }

    vfs_node_t ptmx = vfs_node_alloc(devfs_root, "ptmx");
    ptmx->fsid      = ptmx_fsid;
}

void pty_init() {
    id_allocator_create(MAX_PTY_DEVICE);
    setup_ptmx_device();
    errno_t status = vfs_mkdir("/dev/pts");
    if (status != EOK) {
        kerror("Cannot create /dev/pts.");
        return;
    }
}
