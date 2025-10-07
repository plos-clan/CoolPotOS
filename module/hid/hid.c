#include "hid.h"
#include "proc_subsystem.h"

static bool ctrlPressed  = false;
static bool shiftPressed = false;

// Mapping from USB key id to ps2 key sequence.
static uint16_t key_to_scan_code[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x001e, 0x0030, 0x002e, 0x0020, 0x0012, 0x0021, 0x0022, 0x0023,
    0x0017, 0x0024, 0x0025, 0x0026, 0x0032, 0x0031, 0x0018, 0x0019, 0x0010, 0x0013, 0x001f, 0x0014,
    0x0016, 0x002f, 0x0011, 0x002d, 0x0015, 0x002c, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x001c, 0x0001, 0x000e, 0x000f, 0x0039, 0x000c, 0x000d, 0x001a,
    0x001b, 0x002b, 0x0000, 0x0027, 0x0028, 0x0029, 0x0033, 0x0034, 0x0035, 0x003a, 0x003b, 0x003c,
    0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0057, 0x0058, 0xe037, 0x0046,
    0xe145, 0xe052, 0xe047, 0xe049, 0xe053, 0xe04f, 0xe051, 0xe04d, 0xe04b, 0xe050, 0xe048, 0x0045,
    0xe035, 0x0037, 0x004a, 0x004e, 0xe01c, 0x004f, 0x0050, 0x0051, 0x004b, 0x004c, 0x004d, 0x0047,
    0x0048, 0x0049, 0x0052, 0x0053};

// Mapping from USB modifier id to ps2 key sequence.
static uint16_t modifier_to_scan_code[] = {
    // lcntl, lshift, lalt, lgui, rcntl, rshift, ralt, rgui
    0x001d, 0x002a, 0x0038, 0xe05b, 0xe01d, 0x0036, 0xe038, 0xe05c};

void keyboard_callback(hid_event_t *event, void *user_data) {
    usb_hid_device_t *hid = user_data;

    hid_key_event_t *key = &event->key;

    bool shift = (key->modifiers & (0x02 | 0x20)) != 0;
    if (shift && !shiftPressed) {
        keyboard_scancode(0x2A, 0, 0);
        shiftPressed = true;
    } else if (!shift && shiftPressed) {
        keyboard_scancode(0xAA, 0, 0);
        shiftPressed = false;
    }

    if (event->type == HID_EVENT_KEY_PRESS) {
        // New key pressed.
        uint16_t scancode = key_to_scan_code[key->keycode];
        char     k        = keyboard_scancode(scancode, 0, 0);
        char     tmp[2];
        tmp[0] = k;
        tmp[1] = '\0';
        keyboard_tmp_writer(tmp);
    } else if (event->type == HID_EVENT_KEY_RELEASE) {
        uint16_t scancode = key_to_scan_code[key->keycode];
        char     k        = keyboard_scancode(0x80 | scancode, 0, 0);
    }
}

void mouse_callback(hid_event_t *event, void *user_data) {
    usb_hid_device_t *hid = user_data;
}

// 全局 HID 设备列表
static usb_hid_device_t *hid_devices      = NULL;
static int               hid_device_count = 0;

// 获取 HID 描述符
static int hid_get_descriptor(usb_hid_device_t *hid, uint8_t type, void *buffer, uint16_t length) {
    usb_device_request_t setup = {.bmRequestType =
                                      USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE,
                                  .bRequest = USB_REQ_GET_DESCRIPTOR,
                                  .wValue   = (type << 8) | 0,
                                  .wIndex   = hid->interface_number,
                                  .wLength  = length};

    return usb_control_transfer(hid->usb_device, &setup, buffer, length, NULL, NULL);
}

// 设置协议
int hid_set_protocol(usb_hid_device_t *hid, uint8_t protocol) {
    printk("HID: Setting protocol to %s\n", protocol == HID_PROTOCOL_BOOT ? "Boot" : "Report");

    usb_device_request_t setup = {.bmRequestType =
                                      USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                                  .bRequest = HID_REQ_SET_PROTOCOL,
                                  .wValue   = protocol,
                                  .wIndex   = hid->interface_number,
                                  .wLength  = 0};

    return usb_control_transfer(hid->usb_device, &setup, NULL, 0, NULL, NULL);
}

// 设置空闲
int hid_set_idle(usb_hid_device_t *hid, uint8_t duration) {
    printk("HID: Setting idle duration to %d ms\n", duration * 4);

    usb_device_request_t setup = {
        .bmRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
        .bRequest      = HID_REQ_SET_IDLE,
        .wValue        = (duration << 8) | 0, // Duration in high byte, Report ID in low
        .wIndex        = hid->interface_number,
        .wLength       = 0};

    return usb_control_transfer(hid->usb_device, &setup, NULL, 0, NULL, NULL);
}

static void hid_interrupt_callback(usb_transfer_t *transfer);

bool hid_transfer_done = false;

void hid_resubmit_agent(uint64_t arg) {
    usb_hid_device_t *hid = (usb_hid_device_t *)arg;

    while (1) {
        // 重新提交传输以继续接收
        if (!hid->transfer_active) { break; }

        usb_interrupt_transfer(hid->usb_device, hid->interrupt_in_ep, hid->input_buffer,
                               hid->input_buffer_size, hid_interrupt_callback, hid);

        while (!hid_transfer_done)
            scheduler_yield();

        hid_transfer_done = false;
    }

    task_exit(0);
}

// 中断传输回调
static void hid_interrupt_callback(usb_transfer_t *transfer) {
    usb_hid_device_t *hid = (usb_hid_device_t *)transfer->user_data;

    if (transfer->status != 0) {
        printk("HID: Interrupt transfer failed: %d\n", transfer->status);
        hid->transfer_active = false;
        return;
    }

    if (transfer->actual_length == 0) {
        printk("HID: Empty interrupt transfer\n");
        hid_transfer_done = true;
    }

    // 根据设备类型处理数据
    if (hid->device_type == HID_TYPE_KEYBOARD) {
        hid_keyboard_report_t *report = (hid_keyboard_report_t *)transfer->buffer;

        // 保存当前报告
        memcpy(&hid->prev_keyboard_report, &hid->keyboard_report, sizeof(hid_keyboard_report_t));
        memcpy(&hid->keyboard_report, report, sizeof(hid_keyboard_report_t));

        // 检测按键变化并生成事件
        if (hid->event_callback) {
            // 检测新按下的键
            for (int i = 0; i < 6; i++) {
                uint8_t key = report->keys[i];
                if (key == 0) continue;

                // 检查是否是新按键
                bool is_new = true;
                for (int j = 0; j < 6; j++) {
                    if (hid->prev_keyboard_report.keys[j] == key) {
                        is_new = false;
                        break;
                    }
                }

                if (is_new) {
                    hid_event_t event = {
                        .type = HID_EVENT_KEY_PRESS,
                        .key  = {.keycode = key, .modifiers = report->modifiers, .pressed = true}
                    };

                    hid->event_callback(&event, hid->callback_user_data);
                }
            }

            // 检测释放的键
            for (int i = 0; i < 6; i++) {
                uint8_t key = hid->prev_keyboard_report.keys[i];
                if (key == 0) continue;

                // 检查键是否仍然按下
                bool still_pressed = false;
                for (int j = 0; j < 6; j++) {
                    if (report->keys[j] == key) {
                        still_pressed = true;
                        break;
                    }
                }

                if (!still_pressed) {
                    hid_event_t event = {
                        .type = HID_EVENT_KEY_RELEASE,
                        .key  = {.keycode = key, .modifiers = report->modifiers, .pressed = false}
                    };

                    hid->event_callback(&event, hid->callback_user_data);
                }
            }
        }
    } else if (hid->device_type == HID_TYPE_MOUSE) {
        hid_mouse_report_t *report = (hid_mouse_report_t *)transfer->buffer;

        // 保存报告
        memcpy(&hid->prev_mouse_report, &hid->mouse_report, sizeof(hid_mouse_report_t));
        memcpy(&hid->mouse_report, report,
               transfer->actual_length < sizeof(hid_mouse_report_t) ? transfer->actual_length
                                                                    : sizeof(hid_mouse_report_t));

        // 生成鼠标事件
        if (hid->event_callback) {
            // 移动事件
            if (report->x != 0 || report->y != 0 || report->wheel != 0) {
                hid_event_t event = {
                    .type  = HID_EVENT_MOUSE_MOVE,
                    .mouse = {.x       = report->x,
                              .y       = report->y,
                              .wheel   = report->wheel,
                              .buttons = report->buttons}
                };

                hid->event_callback(&event, hid->callback_user_data);
            }

            // 按键事件
            if (report->buttons != hid->prev_mouse_report.buttons) {
                hid_event_t event = {
                    .type  = HID_EVENT_MOUSE_BUTTON,
                    .mouse = {.x = 0, .y = 0, .wheel = 0, .buttons = report->buttons}
                };

                hid->event_callback(&event, hid->callback_user_data);
            }
        }
    }

    hid_transfer_done = true;
}

// 启动中断传输
static int hid_start_interrupt_transfer(usb_hid_device_t *hid) {
    printk("HID: Starting interrupt transfers\n");

    hid->transfer_active = true;

    task_create("hit resubmit agent", hid_resubmit_agent, (uint64_t)hid, KTHREAD_PRIORITY);

    printk("HID: Interrupt transfers started\n");
    return 0;
}

// 探测 HID 设备
int usb_hid_probe(usb_device_t *device) {
    if (!device) { return -1; }

    printk("\n========== USB HID Device Probe ==========\n");

    // 检查设备类
    bool is_hid = false;

    if (device->descriptor.bDeviceClass == USB_CLASS_HID) {
        is_hid = true;
        printk("HID: Device-level HID class\n");
    }

    // 解析配置描述符查找 HID 接口
    if (!device->config_descriptor) {
        printk("HID: No configuration descriptor\n");
        return -1;
    }

    uint8_t *ptr  = (uint8_t *)device->config_descriptor;
    uint8_t *end  = ptr + device->config_descriptor->wTotalLength;
    ptr          += sizeof(usb_config_descriptor_t);

    usb_interface_descriptor_t *hid_iface    = NULL;
    hid_descriptor_t           *hid_desc_ptr = NULL;
    usb_endpoint_descriptor_t  *interrupt_in = NULL;

    while (ptr < end) {
        uint8_t len  = ptr[0];
        uint8_t type = ptr[1];

        if (len == 0) break;
        if (ptr + len > end) break;

        if (type == USB_DT_INTERFACE) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)ptr;

            if (iface->bInterfaceClass == USB_CLASS_HID) {
                hid_iface = iface;
                is_hid    = true;

                printk("HID: Found HID interface %d\n", iface->bInterfaceNumber);
                printk("  Subclass: 0x%02x (%s)\n", iface->bInterfaceSubClass,
                       iface->bInterfaceSubClass == HID_SUBCLASS_BOOT ? "Boot" : "None");
                printk("  Protocol: 0x%02x (%s)\n", iface->bInterfaceProtocol,
                       iface->bInterfaceProtocol == HID_PROTOCOL_KEYBOARD ? "Keyboard"
                       : iface->bInterfaceProtocol == HID_PROTOCOL_MOUSE  ? "Mouse"
                                                                          : "None");
            }
        } else if (type == HID_DT_HID && hid_iface) {
            hid_desc_ptr = (hid_descriptor_t *)ptr;

            printk("HID Descriptor:\n");
            printk("  bcdHID: 0x%04x\n", hid_desc_ptr->bcdHID);
            printk("  Country: %d\n", hid_desc_ptr->bCountryCode);
            printk("  Descriptors: %d\n", hid_desc_ptr->bNumDescriptors);
            printk("  Report Desc Type: 0x%02x\n", hid_desc_ptr->bDescriptorType2);
            printk("  Report Desc Length: %d\n", hid_desc_ptr->wDescriptorLength);

        } else if (type == USB_DT_ENDPOINT && hid_iface) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)ptr;

            if ((ep->bmAttributes & 0x03) == USB_ENDPOINT_XFER_INT) {
                if (ep->bEndpointAddress & 0x80) {
                    interrupt_in = ep;
                    printk("  Interrupt IN: 0x%02x, MaxPacket=%d, Interval=%d\n",
                           ep->bEndpointAddress, ep->wMaxPacketSize, ep->bInterval);
                }
            }
        }

        ptr += len;
    }

    if (!is_hid || !hid_iface || !interrupt_in) {
        printk("HID: Required descriptors not found\n");
        return -1;
    }

    // 创建 HID 设备结构
    usb_hid_device_t *hid = (usb_hid_device_t *)malloc(sizeof(usb_hid_device_t));
    if (!hid) {
        printk("HID: Failed to allocate device structure\n");
        return -1;
    }

    memset(hid, 0, sizeof(usb_hid_device_t));
    hid->usb_device              = device;
    hid->interface_number        = hid_iface->bInterfaceNumber;
    hid->interrupt_in_ep         = interrupt_in->bEndpointAddress;
    hid->interrupt_in_max_packet = interrupt_in->wMaxPacketSize;
    hid->interrupt_interval      = interrupt_in->bInterval;

    if (hid_desc_ptr) { memcpy(&hid->hid_desc, hid_desc_ptr, sizeof(hid_descriptor_t)); }

    // 确定设备类型
    if (hid_iface->bInterfaceProtocol == HID_PROTOCOL_KEYBOARD) {
        hid->device_type = HID_TYPE_KEYBOARD;
        hid->protocol    = HID_PROTOCOL_KEYBOARD;
        usb_hid_set_event_callback(hid, keyboard_callback, hid);
        printk("HID: Device type: KEYBOARD\n");
    } else if (hid_iface->bInterfaceProtocol == HID_PROTOCOL_MOUSE) {
        hid->device_type = HID_TYPE_MOUSE;
        hid->protocol    = HID_PROTOCOL_MOUSE;
        usb_hid_set_event_callback(hid, mouse_callback, hid);
        printk("HID: Device type: MOUSE\n");
    } else {
        hid->device_type = HID_TYPE_GENERIC;
        printk("HID: Device type: GENERIC\n");
    }

    // 分配输入缓冲区
    hid->input_buffer_size = hid->interrupt_in_max_packet;
    hid->input_buffer      = (uint8_t *)malloc(hid->input_buffer_size);
    if (!hid->input_buffer) {
        printk("HID: Failed to allocate input buffer\n");
        free(hid);
        return -1;
    }

    // 设置 Boot Protocol（如果支持）
    if (hid_iface->bInterfaceSubClass == HID_SUBCLASS_BOOT) {
        printk("HID: Setting Boot Protocol\n");
        hid_set_protocol(hid, HID_PROTOCOL_BOOT);
    }

    // 设置空闲时间（0 = 无限期）
    hid_set_idle(hid, 0);

    // 启动中断传输
    if (hid_start_interrupt_transfer(hid) != 0) {
        printk("HID: Failed to start interrupt transfers\n");
        free(hid->input_buffer);
        free(hid);
        return -1;
    }

    // 添加到设备列表
    hid->next   = hid_devices;
    hid_devices = hid;
    hid_device_count++;

    return 0;
}

// 移除 HID 设备
void usb_hid_remove(usb_hid_device_t *hid) {
    if (!hid) return;

    printk("HID: Removing device\n");

    // 停止传输
    hid->transfer_active = false;

    // 从列表移除
    usb_hid_device_t **prev = &hid_devices;
    while (*prev) {
        if (*prev == hid) {
            *prev = hid->next;
            hid_device_count--;
            break;
        }
        prev = &(*prev)->next;
    }

    if (hid->input_buffer) { free(hid->input_buffer); }

    free(hid);
}

// 设置事件回调
void usb_hid_set_event_callback(usb_hid_device_t *hid, hid_event_callback_t callback,
                                void *user_data) {
    if (!hid) return;

    hid->event_callback     = callback;
    hid->callback_user_data = user_data;
}

// 获取 HID 设备
usb_hid_device_t *usb_hid_get_device(int index) {
    usb_hid_device_t *hid = hid_devices;
    int               i   = 0;

    while (hid && i < index) {
        hid = hid->next;
        i++;
    }

    return hid;
}

usb_hid_device_t *usb_hid_get_keyboard(void) {
    usb_hid_device_t *hid = hid_devices;

    while (hid) {
        if (hid->device_type == HID_TYPE_KEYBOARD) { return hid; }
        hid = hid->next;
    }

    return NULL;
}

usb_hid_device_t *usb_hid_get_mouse(void) {
    usb_hid_device_t *hid = hid_devices;

    while (hid) {
        if (hid->device_type == HID_TYPE_MOUSE) { return hid; }
        hid = hid->next;
    }

    return NULL;
}

int hid_probe(usb_device_t *usbdev) {
    return usb_hid_probe(usbdev);
}

int hid_remove(usb_device_t *usbdev) {
    usb_hid_remove((usb_hid_device_t *)usbdev->private_data);
    return 0;
}

usb_driver_t hid_driver = {
    .class    = USB_CLASS_HID,
    .subclass = 0x00,
    .probe    = hid_probe,
    .remove   = hid_remove,
};

__attribute__((visibility("default"))) int dlmain() {
    register_usb_driver(&hid_driver);

    return 0;
}
