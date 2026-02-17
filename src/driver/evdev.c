#include "driver/evdev.h"
#include "driver/input_device.h"
#include "errno.h"
#include "fs/devtmpfs.h"
#include "krlibc.h"
#include "lib/sprintf.h"
#include "mem/heap.h"
#include "string_builder.h"
#include "task/poll.h"
#include "task/scheduler.h"
#include "term/klog.h"

static evdev_ctx_t evdev_ctx;
static bool        evdev_initialized        = false;
static bool        evdev_handler_registered = false;

// PS/2 Set 1 scancode -> Linux keycode (basic keys, 1:1 mapping for most)
static const uint16_t ps2_to_linux[128] = {
    [0] = KEY_RESERVED,    [1] = KEY_ESC,       [2] = KEY_1,           [3] = KEY_2,
    [4] = KEY_3,           [5] = KEY_4,         [6] = KEY_5,           [7] = KEY_6,
    [8] = KEY_7,           [9] = KEY_8,         [10] = KEY_9,          [11] = KEY_0,
    [12] = KEY_MINUS,      [13] = KEY_EQUAL,    [14] = KEY_BACKSPACE,  [15] = KEY_TAB,
    [16] = KEY_Q,          [17] = KEY_W,        [18] = KEY_E,          [19] = KEY_R,
    [20] = KEY_T,          [21] = KEY_Y,        [22] = KEY_U,          [23] = KEY_I,
    [24] = KEY_O,          [25] = KEY_P,        [26] = KEY_LEFTBRACE,  [27] = KEY_RIGHTBRACE,
    [28] = KEY_ENTER,      [29] = KEY_LEFTCTRL, [30] = KEY_A,          [31] = KEY_S,
    [32] = KEY_D,          [33] = KEY_F,        [34] = KEY_G,          [35] = KEY_H,
    [36] = KEY_J,          [37] = KEY_K,        [38] = KEY_L,          [39] = KEY_SEMICOLON,
    [40] = KEY_APOSTROPHE, [41] = KEY_GRAVE,    [42] = KEY_LEFTSHIFT,  [43] = KEY_BACKSLASH,
    [44] = KEY_Z,          [45] = KEY_X,        [46] = KEY_C,          [47] = KEY_V,
    [48] = KEY_B,          [49] = KEY_N,        [50] = KEY_M,          [51] = KEY_COMMA,
    [52] = KEY_DOT,        [53] = KEY_SLASH,    [54] = KEY_RIGHTSHIFT, [55] = KEY_KPASTERISK,
    [56] = KEY_LEFTALT,    [57] = KEY_SPACE,    [58] = KEY_CAPSLOCK,   [59] = KEY_F1,
    [60] = KEY_F2,         [61] = KEY_F3,       [62] = KEY_F4,         [63] = KEY_F5,
    [64] = KEY_F6,         [65] = KEY_F7,       [66] = KEY_F8,         [67] = KEY_F9,
    [68] = KEY_F10,        [69] = KEY_NUMLOCK,  [70] = KEY_SCROLLLOCK, [71] = KEY_KP7,
    [72] = KEY_KP8,        [73] = KEY_KP9,      [74] = KEY_KPMINUS,    [75] = KEY_KP4,
    [76] = KEY_KP5,        [77] = KEY_KP6,      [78] = KEY_KPPLUS,     [79] = KEY_KP1,
    [80] = KEY_KP2,        [81] = KEY_KP3,      [82] = KEY_KP0,        [83] = KEY_KPDOT,
    [87] = KEY_F11,        [88] = KEY_F12,
};

// PS/2 Set 1 extended scancode (0xE0 prefix) -> Linux keycode
static const uint16_t ps2_ext_to_linux[128] = {
    [0x1C] = KEY_KPENTER, [0x1D] = KEY_RIGHTCTRL, [0x35] = KEY_KPSLASH, [0x38] = KEY_RIGHTALT,
    [0x47] = KEY_HOME,    [0x48] = KEY_UP,        [0x49] = KEY_PAGEUP,  [0x4B] = KEY_LEFT,
    [0x4D] = KEY_RIGHT,   [0x4F] = KEY_END,       [0x50] = KEY_DOWN,    [0x51] = KEY_PAGEDOWN,
    [0x52] = KEY_INSERT,  [0x53] = KEY_DELETE,
};

static void evdev_push_event(uint16_t type, uint16_t code, int32_t value) {
    uint64_t ns = nano_time();

    struct input_event ev;
    ev.time.tv_sec  = (long)(ns / 1000000000ULL);
    ev.time.tv_usec = (long)((ns % 1000000000ULL) / 1000ULL);
    ev.type         = type;
    ev.code         = code;
    ev.value        = value;

    spin_lock(evdev_ctx.lock);
    if (evdev_ctx.count < EVDEV_BUF_SIZE) {
        evdev_ctx.buf[evdev_ctx.tail] = ev;
        evdev_ctx.tail                = (evdev_ctx.tail + 1) % EVDEV_BUF_SIZE;
        evdev_ctx.count++;
    }
    // else: drop event if buffer full
    spin_unlock(evdev_ctx.lock);
}

static bool is_modifier_keycode(uint16_t keycode) {
    return keycode == KEY_LEFTSHIFT || keycode == KEY_RIGHTSHIFT || keycode == KEY_LEFTCTRL
           || keycode == KEY_RIGHTCTRL || keycode == KEY_LEFTALT || keycode == KEY_RIGHTALT
           || keycode == KEY_CAPSLOCK || keycode == KEY_NUMLOCK || keycode == KEY_SCROLLLOCK;
}

static void evdev_input_handler(indev_t *device, intype type, uint64_t code, uint8_t value) {
    if (type != EV_KEY)
        return;
    if (evdev_ctx.open_count <= 0)
        return;

    uint16_t keycode;
    if (code & EVDEV_EXT_FLAG) {
        // Extended key (0xE0 prefix)
        uint8_t idx = code & 0x7F;
        keycode     = (idx < 128) ? ps2_ext_to_linux[idx] : 0;
    } else {
        // Regular key
        uint8_t idx = code & 0x7F;
        keycode     = (idx < 128) ? ps2_to_linux[idx] : 0;
    }

    if (keycode == KEY_RESERVED)
        return;

    // value: EV_PRESS(0) = key press, EV_RELEASE(1) = key release
    // Linux evdev: value 1 = press, value 0 = release
    int32_t ev_value = (value == EV_PRESS) ? 1 : 0;

    // Only send release events for modifier keys (shift, ctrl, alt, etc.)
    // Non-modifier release events are suppressed to avoid terminal emulators
    // that process both press and release as input, causing double characters
    if (ev_value == 0 && !is_modifier_keycode(keycode))
        return;

    evdev_push_event(INPUT_EV_KEY, keycode, ev_value);
    evdev_push_event(INPUT_EV_SYN, SYN_REPORT, 0);
}

static size_t evdev_read(void *handle, void *addr, size_t offset, size_t size) {
    (void)offset;
    size_t ev_size = sizeof(struct input_event);
    if (size < ev_size)
        return 0;

    size_t max_events = size / ev_size;
    size_t read_count = 0;

    // Block until at least one event is available
    for (;;) {
        spin_lock(evdev_ctx.lock);
        if (evdev_ctx.count > 0) {
            while (evdev_ctx.count > 0 && read_count < max_events) {
                memcpy(
                    (uint8_t *)addr + read_count * ev_size, &evdev_ctx.buf[evdev_ctx.head], ev_size
                );
                evdev_ctx.head = (evdev_ctx.head + 1) % EVDEV_BUF_SIZE;
                evdev_ctx.count--;
                read_count++;
            }
            spin_unlock(evdev_ctx.lock);
            return read_count * ev_size;
        }
        spin_unlock(evdev_ctx.lock);
        scheduler_yield();
    }
}

static size_t evdev_write(void *handle, const void *addr, size_t offset, size_t size) {
    (void)handle;
    (void)addr;
    (void)offset;
    (void)size;
    return 0;
}

static int evdev_poll(void *handle, size_t events) {
    int out = 0;
    spin_lock(evdev_ctx.lock);
    if ((events & EPOLLIN) && evdev_ctx.count > 0)
        out |= EPOLLIN;
    spin_unlock(evdev_ctx.lock);
    return out;
}

static errno_t evdev_ioctl(void *handle, size_t req, void *arg) {
    uint32_t cmd  = (uint32_t)req; // mask to 32-bit (may be sign-extended)
    uint8_t  type = _IOC_TYPE(cmd);
    uint8_t  nr   = _IOC_NR(cmd);
    size_t   sz   = _IOC_SIZE(cmd);

    if (type != EVDEV_IOC_TYPE)
        return -ENOSYS;

    switch (nr) {
    case EVIOC_NR_GVERSION: {
        // EVIOCGVERSION: return evdev protocol version
        if (arg == NULL || sz < sizeof(int))
            return -EINVAL;
        *(int *)arg = EV_VERSION;
        return 0;
    }
    case EVIOC_NR_GID: {
        // EVIOCGID: return input device ID
        if (arg == NULL || sz < sizeof(struct input_id))
            return -EINVAL;
        struct input_id *id = (struct input_id *)arg;
        id->bustype         = BUS_I8042;
        id->vendor          = 0;
        id->product         = 0;
        id->version         = 0;
        return 0;
    }
    case EVIOC_NR_GNAME: {
        // EVIOCGNAME: return device name
        const char *name = "PS/2 Keyboard";
        size_t      len  = strlen(name);
        if (arg == NULL || sz == 0)
            return -EINVAL;
        size_t copy = (len + 1 < sz) ? len + 1 : sz;
        memcpy(arg, name, copy);
        if (copy <= len)
            ((char *)arg)[copy - 1] = '\0';
        return (errno_t)copy;
    }
    case EVIOC_NR_GPHYS: {
        // EVIOCGPHYS: return physical path
        const char *phys = "isa0060/serio0/input0";
        size_t      len  = strlen(phys);
        if (arg == NULL || sz == 0)
            return -EINVAL;
        size_t copy = (len + 1 < sz) ? len + 1 : sz;
        memcpy(arg, phys, copy);
        if (copy <= len)
            ((char *)arg)[copy - 1] = '\0';
        return (errno_t)copy;
    }
    case EVIOC_NR_GUNIQ:
    case EVIOC_NR_GPROP: {
        // Return empty / no properties
        if (arg == NULL || sz == 0)
            return -EINVAL;
        memset(arg, 0, sz);
        return 0;
    }
    default:
        break;
    }

    // EVIOCGBIT(ev, len): nr = 0x20 + ev
    if (nr >= EVIOC_NR_GBIT_BASE && nr < EVIOC_NR_GABS_BASE) {
        uint8_t ev = nr - EVIOC_NR_GBIT_BASE;
        if (arg == NULL || sz == 0)
            return -EINVAL;
        memset(arg, 0, sz);

        if (ev == 0) {
            // Event type bitmask: we support EV_SYN(0) and EV_KEY(1)
            uint8_t *bits = (uint8_t *)arg;
            bits[0]       = 0x03; // bit 0 and bit 1
            return 0;
        } else if (ev == INPUT_EV_KEY) {
            // Key code bitmask: set bits for all supported keycodes
            uint8_t *bits     = (uint8_t *)arg;
            size_t   max_bits = sz * 8;

            // Set bits from ps2_to_linux table
            for (int i = 0; i < 128; i++) {
                uint16_t kc = ps2_to_linux[i];
                if (kc != KEY_RESERVED && kc < max_bits)
                    bits[kc / 8] |= (1 << (kc % 8));
            }
            // Set bits from ps2_ext_to_linux table
            for (int i = 0; i < 128; i++) {
                uint16_t kc = ps2_ext_to_linux[i];
                if (kc != KEY_RESERVED && kc < max_bits)
                    bits[kc / 8] |= (1 << (kc % 8));
            }
            return 0;
        }
        // Other event types: return all zeros (no support)
        return 0;
    }

    return -ENOSYS;
}

static void evdev_open(void *parent, const char *path, vfs_node_t node) {
    dtmp_handle_t *dtmp = (dtmp_handle_t *)node->handle;
    if (dtmp)
        dtmp->device_handle = &evdev_ctx;

    spin_lock(evdev_ctx.lock);
    // Flush stale events on first open
    if (evdev_ctx.open_count == 0) {
        evdev_ctx.head  = 0;
        evdev_ctx.tail  = 0;
        evdev_ctx.count = 0;
    }
    evdev_ctx.open_count++;
    spin_unlock(evdev_ctx.lock);
}

static bool evdev_close(void *handle) {
    spin_lock(evdev_ctx.lock);
    evdev_ctx.open_count = 0;
    spin_unlock(evdev_ctx.lock);
    return false;
}

void evdev_setup(vfs_node_t dev_root) {
    if (!evdev_initialized) {
        memset(&evdev_ctx, 0, sizeof(evdev_ctx));
        evdev_initialized = true;
    }

    // Create /dev/input/ directory
    char             *full_path = vfs_get_fullpath(dev_root);
    string_builder_t *path      = create_string_builder(50);
    string_builder_append(path, "%s/input", full_path);
    vfs_mkdir(path->data);
    vfs_node_t input_dir = vfs_open(path->data);

    // Create /dev/input/event0 device node
    create_device_node_ex(
        input_dir, "event0", device_stream, &evdev_ctx, 0, evdev_open, evdev_close,
        (vfs_ioctl_t)evdev_ioctl, (vfs_read_t)evdev_read, (vfs_write_t)evdev_write,
        (vfs_poll_t)evdev_poll, NULL, NULL
    );

    vfs_close(input_dir);
    free(full_path);
    free(path->data);
    free(path);

    // Register input handler only once. devtmpfs can be mounted multiple times
    // (e.g. after switch_root), and duplicate registrations will duplicate
    // every keyboard event in /dev/input/event0.
    if (!evdev_handler_registered) {
        input_handler_t *handler = calloc(1, sizeof(input_handler_t));
        handler->handle          = evdev_input_handler;
        handler->id              = INPUT_KEYBOARD_ID;
        register_input_handler(handler);
        evdev_handler_registered = true;
    }

    kinfo("evdev: registered /dev/input/event0 (keyboard)");
}
