#include "driver/usb/class/hid/mouse.h"
#include "driver/usb/class/hid/common.h"
#include "driver/usb/class/hid/parser.h"
#include "driver/usb/defs/defs.h"
#include "driver/usb/bus/device.h"
#include "krlibc.h"
#include "mem/alloc/alloc.h"
#include "term/klog.h"

typedef struct MouseLayout {
    bool     has_axis_x;
    bool     has_axis_y;
    bool     has_axis_wheel;
    HidField axis_x;
    HidField axis_y;
    HidField axis_wheel;
    HidFieldVec buttons;
} MouseLayout;

typedef struct Mouse {
    UsbDriver driver;
    HidDevice hid;
    MouseLayout layout;
} Mouse;

static void mouse_disconnect(UsbDriver *driver) {
    Mouse *mouse = container_of(driver, Mouse, driver);
    kinfo("Mouse: disconnected");
    hid_device_free(&mouse->hid);
    HidFieldVec_free(&mouse->layout.buttons);
    free(mouse);
}

static void mouse_handle_completion(UsbDriver *driver, CompletionEvent event) {
    Mouse *mouse = container_of(driver, Mouse, driver);

    if (event.ep_addr != mouse->hid.ep_addr) {
        return;
    }

    if (event.status != TRANSFER_STATUS_COMPLETED) {
        kwarn("Mouse: transfer failed (%d)", event.status);
        return;
    }

    const uint8_t *data = mouse->hid.buf_virt;

    if (mouse->layout.has_axis_x) {
        int32_t dx = hid_field_value_signed(&mouse->layout.axis_x, data, 0);
        kinfo("Mouse (X): %d", dx);
    }

    if (mouse->layout.has_axis_y) {
        int32_t dy = hid_field_value_signed(&mouse->layout.axis_y, data, 0);
        kinfo("Mouse (Y): %d", dy);
    }

    if (mouse->layout.has_axis_wheel) {
        int32_t wheel = hid_field_value_signed(&mouse->layout.axis_wheel, data, 0);
        kinfo("Mouse (Wheel): %d", wheel);
    }

    for (size_t i = 0; i < mouse->layout.buttons.len; i++) {
        HidField *field = &mouse->layout.buttons.data[i];
        for (uint32_t j = 0; j < field->report_count; j++) {
            if (hid_field_value(field, data, j) != 0) {
                uint32_t btn_id = (field->usage_min + j) & 0xffff;
                kinfo("Mouse (Btn): %d", btn_id);
            }
        }
    }

    hid_device_submit_transfer(&mouse->hid);
}

static void mouse_scan_layout(Mouse *mouse) {
    for (uint32_t i = 0; i < 256; i++) {
        if (!mouse->hid.descriptor.reports.used[i]) {
            continue;
        }
        HidReport *report = &mouse->hid.descriptor.reports.values[i];
        if (hid_report_size_bytes(report, HID_KIND_INPUT) == 0) {
            continue;
        }
        for (size_t j = 0; j < report->fields.len; j++) {
            HidField *field = &report->fields.data[j];
            if (field->kind != HID_KIND_INPUT) {
                continue;
            }
            if (hid_field_is_const(field) || !hid_field_is_variable(field)) {
                continue;
            }
            if (field->usage_page == 0x01) {
                switch (field->usage_min & 0xffff) {
                case 0x30:
                    mouse->layout.axis_x = *field;
                    mouse->layout.has_axis_x = true;
                    break;
                case 0x31:
                    mouse->layout.axis_y = *field;
                    mouse->layout.has_axis_y = true;
                    break;
                case 0x38:
                    mouse->layout.axis_wheel = *field;
                    mouse->layout.has_axis_wheel = true;
                    break;
                default:
                    break;
                }
            } else if (field->usage_page == 0x09) {
                HidFieldVec_push(&mouse->layout.buttons, *field);
            }
        }
    }

    if (!mouse->layout.has_axis_x && mouse->layout.buttons.len == 0) {
        kwarn("Mouse: No mouse fields found");
    }
}

static Mouse *mouse_new(UsbInterface *iface, uint8_t ep_addr) {
    Mouse *mouse = (Mouse *)malloc(sizeof(Mouse));
    if (!mouse) {
        return NULL;
    }
    memset(mouse, 0, sizeof(Mouse));

    if (!hid_device_new(&mouse->hid, iface, ep_addr)) {
        free(mouse);
        return NULL;
    }

    HidFieldVec_init(&mouse->layout.buttons);
    mouse_scan_layout(mouse);

    mouse->driver.disconnect = mouse_disconnect;
    mouse->driver.handle_completion = mouse_handle_completion;

    return mouse;
}

UsbDriver *hid_probe_mouse(UsbInterface *iface) {
    if (!usb_interface_matches(iface, USB_CLASS_HID, 1, 2)) {
        return NULL;
    }

    UsbEndpoint *ep = usb_interface_find_endpoint(iface, USB_EP_TYPE_INT, true);
    if (!ep) {
        kerror("Mouse: No Interrupt IN endpoint");
        return NULL;
    }

    Mouse *mouse = mouse_new(iface, ep->desc.endpoint_address);
    if (!mouse) {
        return NULL;
    }

    kinfo("HID Mouse (Slot %d)", iface->device->slot_id);

    hid_device_submit_transfer(&mouse->hid);
    return &mouse->driver;
}
