#include "driver/usb/class/hid/kbd.h"
#include "driver/input_device.h"
#include "driver/usb/bus/device.h"
#include "driver/usb/class/hid/common.h"
#include "driver/usb/class/hid/parser.h"
#include "driver/usb/defs/defs.h"
#include "krlibc.h"
#include "mem/alloc/alloc.h"
#include "term/klog.h"

typedef struct KeyLayout {
    HidFieldVec modifiers;
    HidFieldVec arrays;
    HidFieldVec bitmaps;
} KeyLayout;

typedef struct Keyboard {
    UsbDriver driver;
    HidDevice hid;
    KeyLayout layout;
    indev_t  *input_dev;
    bool      caps_locked;
    bool      pressed[256];
} Keyboard;

static const char *usb_kbd_escape_from_usage(uint16_t usage) {
    switch (usage) {
    case 0x52:
        return "\x1b[A";
    case 0x51:
        return "\x1b[B";
    case 0x50:
        return "\x1b[D";
    case 0x4F:
        return "\x1b[C";
    case 0x4A:
        return "\x1b[H";
    case 0x4D:
        return "\x1b[F";
    case 0x4B:
        return "\x1b[5~";
    case 0x4E:
        return "\x1b[6~";
    case 0x49:
        return "\x1b[2~";
    case 0x4C:
        return "\x1b[3~";
    default:
        return NULL;
    }
}

static char usb_kbd_ascii_from_usage(uint16_t usage, bool shift, bool caps) {
    if (usage >= 0x04 && usage <= 0x1d) {
        bool upper = shift ^ caps;
        return (char)(upper ? ('A' + (usage - 0x04)) : ('a' + (usage - 0x04)));
    }

    switch (usage) {
    case 0x1E:
        return shift ? '!' : '1';
    case 0x1F:
        return shift ? '@' : '2';
    case 0x20:
        return shift ? '#' : '3';
    case 0x21:
        return shift ? '$' : '4';
    case 0x22:
        return shift ? '%' : '5';
    case 0x23:
        return shift ? '^' : '6';
    case 0x24:
        return shift ? '&' : '7';
    case 0x25:
        return shift ? '*' : '8';
    case 0x26:
        return shift ? '(' : '9';
    case 0x27:
        return shift ? ')' : '0';
    case 0x28:
        return '\n';
    case 0x29:
        return 0x1b;
    case 0x2A:
        return '\b';
    case 0x2B:
        return '\t';
    case 0x2C:
        return ' ';
    case 0x2D:
        return shift ? '_' : '-';
    case 0x2E:
        return shift ? '+' : '=';
    case 0x2F:
        return shift ? '{' : '[';
    case 0x30:
        return shift ? '}' : ']';
    case 0x31:
        return shift ? '|' : '\\';
    case 0x32:
        return shift ? '|' : '\\';
    case 0x33:
        return shift ? ':' : ';';
    case 0x34:
        return shift ? '"' : '\'';
    case 0x35:
        return shift ? '~' : '`';
    case 0x36:
        return shift ? '<' : ',';
    case 0x37:
        return shift ? '>' : '.';
    case 0x38:
        return shift ? '?' : '/';
    default:
        return 0;
    }
}

static void keyboard_disconnect(UsbDriver *driver) {
    Keyboard *kbd = container_of(driver, Keyboard, driver);
    kinfo("KBD: disconnected");
    hid_device_free(&kbd->hid);
    HidFieldVec_free(&kbd->layout.modifiers);
    HidFieldVec_free(&kbd->layout.arrays);
    HidFieldVec_free(&kbd->layout.bitmaps);
    if (kbd->input_dev) {
        delete_input_device(kbd->input_dev);
        kbd->input_dev = NULL;
    }
    free(kbd);
}

static void keyboard_handle_completion(UsbDriver *driver, CompletionEvent event) {
    Keyboard *kbd = container_of(driver, Keyboard, driver);

    if (event.ep_addr != kbd->hid.ep_addr) {
        return;
    }
    if (event.status != TRANSFER_STATUS_COMPLETED) {
        kwarn("KBD: Transfer failed (%d)", event.status);
        return;
    }

    const uint8_t *data = kbd->hid.buf_virt;

    bool shift = false;
    bool ctrl  = false;
    bool now_pressed[256];
    memset(now_pressed, 0, sizeof(now_pressed));

    for (size_t i = 0; i < kbd->layout.modifiers.len; i++) {
        HidField *mod = &kbd->layout.modifiers.data[i];
        if (hid_field_value(mod, data, 0) == 1) {
            uint16_t usage = (uint16_t)(mod->usage_min & 0xffff);
            if (usage < 256) {
                now_pressed[usage] = true;
            }
            if (usage == 0xE1 || usage == 0xE5) {
                shift = true;
            }
            if (usage == 0xE0 || usage == 0xE4) {
                ctrl = true;
            }
        }
    }

    for (size_t i = 0; i < kbd->layout.arrays.len; i++) {
        HidField *arr = &kbd->layout.arrays.data[i];
        for (uint32_t j = 0; j < arr->report_count; j++) {
            uint32_t usage = hid_field_value(arr, data, j);
            if (usage > 1 && usage < 256) {
                now_pressed[usage] = true;
            }
        }
    }

    for (size_t i = 0; i < kbd->layout.bitmaps.len; i++) {
        HidField *bmp = &kbd->layout.bitmaps.data[i];
        if (hid_field_value(bmp, data, 0) == 1) {
            uint16_t usage = (uint16_t)(bmp->usage_min & 0xffff);
            if (usage < 256) {
                now_pressed[usage] = true;
            }
        }
    }

    for (uint16_t usage = 0; usage < 256; usage++) {
        if (now_pressed[usage] && !kbd->pressed[usage]) {
            if (kbd->input_dev) {
                send_input_event(kbd->input_dev, EV_KEY, usage, EV_PRESS);
            }

            if (usage == 0x39) {
                kbd->caps_locked = !kbd->caps_locked;
            } else {
                const char *esc = usb_kbd_escape_from_usage(usage);
                if (esc) {
                    if (kbd->input_dev) {
                        send_input_event(kbd->input_dev, EV_CHAR, (uint64_t)esc, 0);
                    }
                } else {
                    char ch = usb_kbd_ascii_from_usage(usage, shift, kbd->caps_locked);
                    if (ch != 0) {
                        if (ctrl) {
                            ch = (char)(ch & 0x1f);
                        }
                        char out[2];
                        out[0] = ch;
                        out[1] = '\0';
                        if (kbd->input_dev) {
                            send_input_event(kbd->input_dev, EV_CHAR, (uint64_t)out, 0);
                        }
                    }
                }
            }
        } else if (!now_pressed[usage] && kbd->pressed[usage]) {
            if (kbd->input_dev) {
                send_input_event(kbd->input_dev, EV_KEY, usage, EV_RELEASE);
            }
        }
    }

    memcpy(kbd->pressed, now_pressed, sizeof(now_pressed));

    hid_device_submit_transfer(&kbd->hid);
}

static void keyboard_scan_layout(Keyboard *kbd) {
    for (uint32_t i = 0; i < 256; i++) {
        if (!kbd->hid.descriptor.reports.used[i]) {
            continue;
        }
        HidReport *report = &kbd->hid.descriptor.reports.values[i];
        if (hid_report_size_bytes(report, HID_KIND_INPUT) == 0) {
            continue;
        }
        for (size_t j = 0; j < report->fields.len; j++) {
            HidField *field = &report->fields.data[j];
            if (field->kind != HID_KIND_INPUT) {
                continue;
            }
            if (hid_field_is_const(field) || field->usage_page != 0x07) {
                continue;
            }
            if (!hid_field_is_variable(field)) {
                HidFieldVec_push(&kbd->layout.arrays, *field);
                continue;
            }
            uint32_t usage = field->usage_min & 0xffff;
            if (usage >= 0xe0 && usage <= 0xe7) {
                HidFieldVec_push(&kbd->layout.modifiers, *field);
            } else {
                HidFieldVec_push(&kbd->layout.bitmaps, *field);
            }
        }
    }

    if (kbd->layout.modifiers.len == 0 && kbd->layout.arrays.len == 0) {
        kwarn("KBD: No keyboard fields found");
    }
}

static Keyboard *keyboard_new(UsbInterface *iface, uint8_t ep_addr) {
    Keyboard *kbd = (Keyboard *)malloc(sizeof(Keyboard));
    if (!kbd) {
        return NULL;
    }
    memset(kbd, 0, sizeof(Keyboard));

    if (!hid_device_new(&kbd->hid, iface, ep_addr)) {
        free(kbd);
        return NULL;
    }

    HidFieldVec_init(&kbd->layout.modifiers);
    HidFieldVec_init(&kbd->layout.arrays);
    HidFieldVec_init(&kbd->layout.bitmaps);

    keyboard_scan_layout(kbd);

    kbd->input_dev = alloc_input_dev();
    if (kbd->input_dev) {
        kbd->input_dev->id   = INPUT_KEYBOARD_ID;
        kbd->input_dev->name = strdup("");
        register_input_device(kbd->input_dev);
    }

    kbd->driver.disconnect        = keyboard_disconnect;
    kbd->driver.handle_completion = keyboard_handle_completion;

    return kbd;
}

UsbDriver *hid_probe_kbd(UsbInterface *iface) {
    if (!usb_interface_matches(iface, USB_CLASS_HID, 1, 1)) {
        return NULL;
    }

    UsbEndpoint *ep = usb_interface_find_endpoint(iface, USB_EP_TYPE_INT, true);
    if (!ep) {
        kerror("KBD: No Interrupt IN endpoint");
        return NULL;
    }

    Keyboard *kbd = keyboard_new(iface, ep->desc.endpoint_address);
    if (!kbd) {
        return NULL;
    }

    kinfo("HID Keyboard (Slot %d)", iface->device->slot_id);

    hid_device_submit_transfer(&kbd->hid);
    return &kbd->driver;
}
