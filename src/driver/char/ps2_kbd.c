#include "driver/char/ps2_kbd.h"
#include "driver/evdev.h"
#include "driver/input_device.h"
#include "driver/tty.h"
#include "intctl.h"
#include "krlibc.h"
#include "lib/acpica/acpi.h"
#include "term/klog.h"

#if defined(__x86_64__) || defined(__amd64__)
#    include "io.h"
#endif

static uint64_t ps2_kbd_irq = -1;
indev_t        *ps2_kbd_device;
bool            ctrled     = false;
bool            shifted    = false;
bool            capsLocked = false;

char character_table[140] = {
    0,    27,   '1',  '2', '3', '4', '5', '6', '7',  '8',  '9',  '0',  '-',  '=',  0,    9,
    'q',  'w',  'e',  'r', 't', 'y', 'u', 'i', 'o',  'p',  '[',  ']',  0,    0,    'a',  's',
    'd',  'f',  'g',  'h', 'j', 'k', 'l', ';', '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',', '.', '/', 0,   '*', 0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0x1B, 0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0x0E, 0x1C, 0,    0,    0,
    0,    0,    0,    0,   0,   '/', 0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0,   0,   0,   0,   0,   0,    0,    0,    0x2C,
};

char shifted_character_table[140] = {
    0,    27,   '!',  '@', '#', '$', '%', '^', '&',  '*',  '(',  ')',  '_',  '+',  0,    '\t',
    'Q',  'W',  'E',  'R', 'T', 'Y', 'U', 'I', 'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
    'D',  'F',  'G',  'H', 'J', 'K', 'L', ':', '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<', '>', '?', 0,   '*', 0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0x1B, 0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0x08, 0x0A, 0,    0,    0,
    0,    0,    0,    0,   0,   '/', 0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0,   0,   0,   0,   0,   0,    0,    0,    0x2C,
};

char cap_character_table[140] = {
    0,    27,   '1',  '2', '3', '4', '5', '6', '7',  '8',  '9',  '0',  '-',  '=',  0,    9,
    'Q',  'W',  'E',  'R', 'T', 'Y', 'U', 'I', 'O',  'P',  '[',  ']',  0,    0,    'A',  'S',
    'D',  'F',  'G',  'H', 'J', 'K', 'L', ';', '\'', '`',  0,    '\\', 'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  ',', '.', '/', 0,   '*', 0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0x1B, 0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0x0E, 0x1C, 0,    0,    0,
    0,    0,    0,    0,   0,   '/', 0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0,   0,   0,   0,   0,   0,    0,    0,    0x2C,
};

char shifted_cap_character_table[140] = {
    0,    27,   '!',  '@', '#', '$', '%', '^', '&',  '*',  '(',  ')',  '_',  '+',  0,    9,
    'q',  'w',  'e',  'r', 't', 'y', 'u', 'i', 'o',  'p',  '{',  '}',  0,    0,    'a',  's',
    'd',  'f',  'g',  'h', 'j', 'k', 'l', ':', '"',  '~',  0,    '|',  'z',  'x',  'c',  'v',
    'b',  'n',  'm',  '<', '>', '?', 0,   '*', 0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0x1B, 0,    0,    0,   0,   0,   0,   0,   0,    0,    0,    0x0E, 0x1C, 0,    0,    0,
    0,    0,    0,    0,   0,   '?', 0,   0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   0,   0,   0,   0,   0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0,   0,   0,   0,   0,   0,    0,    0,    0x2C,
};

char *character_array[2][2] = {
    { character_table,     shifted_character_table     },
    { cap_character_table, shifted_cap_character_table }
};

extern tty_t *current_session;

uint8_t keyboard_scancode(uint8_t scancode, uint8_t scancode_1, uint8_t scancode_2) {
    if (scancode == 0xE0) {
        switch (scancode_1) {
        case 0x48:
            return KEY_BUTTON_UP;
        case 0x50:
            return KEY_BUTTON_DOWN;
        case 0x4b:
            return KEY_BUTTON_LEFT;
        case 0x4d:
            return KEY_BUTTON_RIGHT;
        case 0x47:
            return KEY_BUTTON_HOME;
        case 0x49:
            return KEY_BUTTON_PDOWN;
        case 0x4f:
            return KEY_BUTTON_END;
        case 0x51:
            return KEY_BUTTON_PUP;
        case 0x52:
            return KEY_BUTTON_INSERT;
        case 0x53:
            return KEY_BUTTON_DEL;
        default:
            return 0;
        }
    }
    if (shifted == 1 && scancode & 0x80) {
        if ((scancode & 0x7F) == SCANCODE_SHIFT_L || (scancode & 0x7F) == SCANCODE_SHIFT_R) {
            shifted = 0;
            return 0;
        }
    }

    if (ctrled == 1 && scancode & 0x80) {
        if ((scancode & 0x7F) == 0x1d) {
            ctrled = false;
            return 0;
        }
    }

    if (scancode < sizeof(character_table) && !(scancode & 0x80)) {
        char character = character_array[capsLocked][shifted][scancode];

        if (character != 0) { // Normal char
            return character;
        }

        switch (scancode) {
        case SCANCODE_ENTER:
            return CHARACTER_ENTER;
        case SCANCODE_BACK:
            return CHARACTER_BACK;
        case SCANCODE_SHIFT_L:
        case SCANCODE_SHIFT_R:
            shifted = true;
            break;
        case 0x1d:
            ctrled = true;
            break;
        case SCANCODE_CAPS:
            capsLocked = !capsLocked;
            break;
        }
    }

    return 0;
}

static void ps2_key_handle(uint64_t irq, void *arg, struct pt_regs *regs) {
    uint8_t scancode = 0;
    char    out      = 0;
#if defined(__x86_64__) || defined(__amd64__)
    scancode = io_in8(0x60);
    if (scancode == 0xE0) {
        uint8_t sc2 = io_in8(0x60);
        send_input_event(
            ps2_kbd_device, EV_KEY, (uint64_t)(sc2 & 0x7F) | EVDEV_EXT_FLAG,
            (sc2 & 0x80) ? EV_RELEASE : EV_PRESS
        );
        out = keyboard_scancode(scancode, sc2, 0);
    } else if (scancode == 0xE1) {
        out = keyboard_scancode(scancode, io_in8(0x60), io_in8(0x60));
    } else {
        send_input_event(
            ps2_kbd_device, EV_KEY, (uint64_t)(scancode & 0x7F),
            (scancode & 0x80) ? EV_RELEASE : EV_PRESS
        );
        out = keyboard_scancode(scancode, 0, 0);
    }
#endif
    char *c;
    switch ((uint8_t)out) {
    case CHARACTER_ENTER:
        c = "\n";
        break;
    case CHARACTER_BACK:
        c = "\b";
        break;
    case KEY_BUTTON_UP:
        c = "\x1b[A";
        break;
    case KEY_BUTTON_DOWN:
        c = "\x1b[B";
        break;
    case KEY_BUTTON_LEFT:
        c = "\x1b[D";
        break;
    case KEY_BUTTON_RIGHT:
        c = "\x1b[C";
        break;
    case KEY_BUTTON_HOME:
        c = "\x1b[H";
        break;
    case KEY_BUTTON_PDOWN:
        c = "\x1b[5~";
        break;
    case KEY_BUTTON_END:
        c = "\x1b[F";
        break;
    case KEY_BUTTON_PUP:
        c = "\x1b[6~";
        break;
    case KEY_BUTTON_INSERT:
        c = "\x1b[2~";
        break;
    case KEY_BUTTON_DEL:
        c = "\x1b[3~";
        break;
    default:
        if (ctrled)
            out &= 0x1f;
        char c0[2];
        c0[0] = out;
        c0[1] = '\0';
        send_input_event(ps2_kbd_device, EV_CHAR, (uint64_t)c0, 0);
        return;
    }
    send_input_event(ps2_kbd_device, EV_CHAR, (uint64_t)c, 0);
}

void ps2k_create_device() {
#if defined(__x86_64__) || defined(__amd64__)
    extern intctl_t apic_controller;
    irq_regist_irq(
        ps2_kbd_irq + IRQ_BASE_VECTOR, ps2_key_handle, ps2_kbd_irq, NULL, &apic_controller,
        "ps2_keyboard", 0, IO_APIC
    );
#endif
    irq_set_alloc(ps2_kbd_irq);
    ps2_kbd_device       = alloc_input_dev();
    ps2_kbd_device->id   = INPUT_KEYBOARD_ID;
    ps2_kbd_device->name = strdup("");
    register_input_device(ps2_kbd_device);
}

typedef struct {
    UINT32 Irq;
    UINT32 IoBase;
} ps2_resource_t;

ACPI_STATUS resource_callback(ACPI_RESOURCE *Resource, void *Context) {
    ps2_resource_t *res = Context;
    switch (Resource->Type) {
    case ACPI_RESOURCE_TYPE_IRQ:
        if (Resource->Data.Irq.InterruptCount > 0) {
            res->Irq = Resource->Data.Irq.Interrupts[0];
        }
        break;
    case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
        if (Resource->Data.ExtendedIrq.InterruptCount > 0) {
            res->Irq = Resource->Data.ExtendedIrq.Interrupts[0];
        }
        break;
    case ACPI_RESOURCE_TYPE_IO:
        res->IoBase = Resource->Data.Io.Minimum;
        break;
    case ACPI_RESOURCE_TYPE_FIXED_IO:
        res->IoBase = Resource->Data.FixedIo.Address;
        break;
    }
    return AE_OK;
}

ACPI_STATUS device_found_callback(
    ACPI_HANDLE ObjectHandle, UINT32 NestingLevel, void *Context, void **ReturnValue
) {
    ACPI_STATUS    status;
    ps2_resource_t res    = { 0 };
    ACPI_BUFFER    buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    status = AcpiEvaluateObjectTyped(ObjectHandle, "_STA", NULL, &buffer, ACPI_TYPE_INTEGER);
    if (ACPI_FAILURE(status)) {
        return AE_ERROR;
    }
    ACPI_OBJECT *obj       = buffer.Pointer;
    UINT64       sta_value = obj->Integer.Value;
    AcpiOsFree(buffer.Pointer);
    if ((sta_value & 0x3) != 0x3) {
        return AE_OK;
    }
    status = AcpiWalkResources(ObjectHandle, "_CRS", resource_callback, &res);
    if (ACPI_FAILURE(status)) {
        return AE_OK;
    }
    char *type = Context;
    kinfo("Found PS/2 %s: IRQ=%d, IO=0x%x", type, res.Irq, res.IoBase);
    ps2_kbd_irq = res.Irq;
    ps2k_create_device();
    return AE_OK;
}

bool has_ps2_controller() {
    if (AcpiGbl_FADT.BootFlags & ACPI_FADT_8042) {
        return true;
    }
    return false;
}

void ps2_kdb_setup() {
    if (!has_ps2_controller())
        return;
    AcpiGetDevices("PNP0303", device_found_callback, "Keyboard", NULL);
}
