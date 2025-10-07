#include "usb_enumeration.h"
#include "heap.h"
#include "kprint.h"

// 全局地址分配计数器
static uint8_t next_device_address = 1;

// 分配设备地址
static uint8_t allocate_device_address(void) {
    uint8_t addr = next_device_address++;
    if (next_device_address > 127) {
        next_device_address = 1; // 回绕
    }
    return addr;
}

// 创建枚举上下文
static usb_enum_context_t *usb_alloc_enum_context(usb_hcd_t *hcd, uint8_t port_id) {
    usb_enum_context_t *ctx = (usb_enum_context_t *)malloc(sizeof(usb_enum_context_t));
    if (!ctx) { return NULL; }

    memset(ctx, 0, sizeof(usb_enum_context_t));
    ctx->hcd         = hcd;
    ctx->port_id     = port_id;
    ctx->state       = USB_ENUM_STATE_IDLE;
    ctx->buffer_size = 1024;

    ctx->buffer = (uint8_t *)malloc(ctx->buffer_size);
    if (!ctx->buffer) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

// 释放枚举上下文
static void usb_free_enum_context(usb_enum_context_t *ctx) {
    if (!ctx) return;

    if (ctx->buffer) { free(ctx->buffer); }

    free(ctx);
}

// 解析配置描述符
int usb_parse_config_descriptor(usb_device_t *device, uint8_t *buffer, uint32_t length) {
    if (length < sizeof(usb_config_descriptor_t)) {
        printk("USB: Config descriptor too short\n");
        return -1;
    }

    usb_config_descriptor_t *config = (usb_config_descriptor_t *)buffer;

    printk("USB: Parsing configuration descriptor:\n");
    printk("  Total Length: %d\n", config->wTotalLength);
    printk("  Num Interfaces: %d\n", config->bNumInterfaces);
    printk("  Config Value: %d\n", config->bConfigurationValue);
    printk("  Attributes: 0x%02x\n", config->bmAttributes);
    printk("  Max Power: %d mA\n", config->bMaxPower * 2);

    // 保存配置描述符
    if (device->config_descriptor) { free(device->config_descriptor); }

    device->config_descriptor = (usb_config_descriptor_t *)malloc(config->wTotalLength);
    if (!device->config_descriptor) { return -1; }

    memcpy(device->config_descriptor, buffer,
           length < config->wTotalLength ? length : config->wTotalLength);

    // 解析接口和端点
    uint8_t *ptr = buffer + sizeof(usb_config_descriptor_t);
    uint8_t *end = buffer + (length < config->wTotalLength ? length : config->wTotalLength);

    int endpoint_index = 1; // 0是控制端点

    while (ptr < end) {
        uint8_t desc_length = ptr[0];
        uint8_t desc_type   = ptr[1];

        if (desc_length == 0) { break; }

        if (ptr + desc_length > end) { break; }

        switch (desc_type) {
        case USB_DT_INTERFACE: {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)ptr;
            printk("  Interface %d:\n", iface->bInterfaceNumber);
            printk("    Class: 0x%02x\n", iface->bInterfaceClass);
            printk("    SubClass: 0x%02x\n", iface->bInterfaceSubClass);
            printk("    Protocol: 0x%02x\n", iface->bInterfaceProtocol);
            printk("    Num Endpoints: %d\n", iface->bNumEndpoints);
            device->class    = iface->bInterfaceClass;
            device->subclass = iface->bInterfaceSubClass;
        } break;

        case USB_DT_ENDPOINT: {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)ptr;
            printk("  Endpoint 0x%02x:\n", ep->bEndpointAddress);
            printk("    Type: %d\n", ep->bmAttributes & 0x03);
            printk("    Max Packet Size: %d\n", ep->wMaxPacketSize);
            printk("    Interval: %d\n", ep->bInterval);

            // 保存端点信息
            if (endpoint_index < 32) {
                device->endpoints[endpoint_index].address         = ep->bEndpointAddress;
                device->endpoints[endpoint_index].attributes      = ep->bmAttributes;
                device->endpoints[endpoint_index].max_packet_size = ep->wMaxPacketSize;
                device->endpoints[endpoint_index].interval        = ep->bInterval;
                device->endpoints[endpoint_index].device          = device;
                endpoint_index++;
            }
        } break;

        default: printk("  Unknown descriptor type: 0x%02x\n", desc_type); break;
        }

        ptr += desc_length;
    }

    return 0;
}

// 配置设备端点
int usb_configure_device_endpoints(usb_device_t *device) {
    if (!device || !device->hcd || !device->hcd->ops->configure_endpoint) { return -1; }

    printk("USB: Configuring device endpoints\n");

    int configured = 0;

    // 跳过端点0（控制端点已经配置）
    for (int i = 1; i < 32; i++) {
        if (device->endpoints[i].address != 0) {
            printk("USB: Configuring endpoint %d (address 0x%02x)\n", i,
                   device->endpoints[i].address);

            int ret = device->hcd->ops->configure_endpoint(device->hcd, &device->endpoints[i]);
            if (ret == 0) {
                configured++;
            } else {
                printk("USB: Failed to configure endpoint %d\n", i);
            }
        }
    }

    printk("USB: Configured %d endpoints\n", configured);
    return configured;
}

// 枚举状态机
int usb_enum_state_machine(usb_enum_context_t *ctx) {
    int ret = 0;

    switch (ctx->state) {
    case USB_ENUM_STATE_IDLE:
        printk("USB: Starting enumeration\n");
        ctx->state = USB_ENUM_STATE_RESET;
        break;

    case USB_ENUM_STATE_RESET:
        if (ctx->hcd->ops->reset_port) {
            ret = ctx->hcd->ops->reset_port(ctx->hcd, ctx->port_id);
            if (ret != 0) {
                printk("USB: Port reset failed\n");
                ctx->state = USB_ENUM_STATE_ERROR;
                return -1;
            }
        }

        ctx->state = USB_ENUM_STATE_RESET_WAIT;
        break;

    case USB_ENUM_STATE_RESET_WAIT:
        // ===== 步骤1：分配设备结构 =====
        if (!ctx->device) {
            ctx->device = usb_alloc_device(ctx->hcd);
            if (!ctx->device) {
                printk("USB: Failed to allocate device\n");
                ctx->state = USB_ENUM_STATE_ERROR;
                return -1;
            }

            // 设置设备速度（从端口状态检测得到）
            // 这个speed应该在调用enumerate时就设置好了
            printk("USB: Device speed: %d\n", ctx->device->speed);
        }

        // ===== 步骤2：Enable Slot (XHCI特定) =====
        ret = ctx->hcd->ops->enable_slot(ctx->hcd, ctx->device);
        if (ret != 0) {
            printk("USB: Failed to enable slot\n");
            usb_free_device(ctx->device);
            ctx->device = NULL;
            ctx->state  = USB_ENUM_STATE_ERROR;
            return -1;
        }

        // 验证EP0已经初始化
        if (!ctx->device->endpoints[0].hcd_private) {
            printk("USB: ERROR - EP0 not initialized after enable_slot\n");
            ctx->state = USB_ENUM_STATE_ERROR;
            return -1;
        }

        ctx->state = USB_ENUM_STATE_ADDRESS;
        break;

    case USB_ENUM_STATE_ADDRESS:
        // ===== 步骤3：分配地址并执行 Address Device 命令 =====
        ctx->address = allocate_device_address();
        printk("USB: Assigning address %d to device\n", ctx->address);

        ret = ctx->hcd->ops->address_device(ctx->hcd, ctx->device, ctx->address);
        if (ret != 0) {
            printk("USB: Failed to address device\n");
            ctx->state = USB_ENUM_STATE_ERROR;
            return -1;
        }

        // 验证EP0仍然有效
        if (!ctx->device->endpoints[0].hcd_private) {
            printk("USB: ERROR - EP0 lost after address_device\n");
            ctx->state = USB_ENUM_STATE_ERROR;
            return -1;
        }

        ctx->state = USB_ENUM_STATE_ADDRESS_WAIT;
        break;

    case USB_ENUM_STATE_ADDRESS_WAIT: ctx->state = USB_ENUM_STATE_GET_DESC_8; break;

    case USB_ENUM_STATE_GET_DESC_8:
        // 验证可以进行控制传输
        if (!ctx->device->endpoints[0].hcd_private) {
            printk("USB: CRITICAL - EP0 not ready for control transfer\n");
            ctx->state = USB_ENUM_STATE_ERROR;
            return -1;
        }

        ret = usb_get_descriptor(ctx->device, USB_DT_DEVICE, 0, ctx->buffer, 8);
        if (ret != 0) {
            printk("USB: Failed to get device descriptor (8 bytes), error=%d\n", ret);
            if (++ctx->retry_count < 3) {
                printk("USB: Retrying... (attempt %d/3)\n", ctx->retry_count + 1);
                break;
            }
            printk("USB: All retries exhausted\n");
            ctx->state = USB_ENUM_STATE_ERROR;
            return -1;
        }

        // 解析最大包大小
        usb_device_descriptor_t *desc = (usb_device_descriptor_t *)ctx->buffer;

        // 更新EP0最大包大小（如果需要）
        if (desc->bMaxPacketSize0 != ctx->device->endpoints[0].max_packet_size) {
            printk("USB: Updating EP0 max packet size: %d -> %d\n",
                   ctx->device->endpoints[0].max_packet_size, desc->bMaxPacketSize0);
            ctx->device->endpoints[0].max_packet_size = desc->bMaxPacketSize0;

            // 注意：在XHCI中，EP0的max packet size在Address Device时已设置
            // 如果不匹配，可能需要重新配置（Evaluate Context命令）
            // 但通常Address Device时我们已经根据速度设置了正确的值
        }

        ctx->retry_count = 0;
        ctx->state       = USB_ENUM_STATE_GET_DEVICE_DESC;
        break;

    case USB_ENUM_STATE_GET_DEVICE_DESC:
        ret = usb_get_descriptor(ctx->device, USB_DT_DEVICE, 0, ctx->buffer,
                                 sizeof(usb_device_descriptor_t));
        if (ret != 0) {
            printk("USB: Failed to get full device descriptor\n");
            if (++ctx->retry_count < 3) { break; }
            ctx->state = USB_ENUM_STATE_ERROR;
            return -1;
        }

        // 保存设备描述符
        memcpy(&ctx->device->descriptor, ctx->buffer, sizeof(usb_device_descriptor_t));

        ctx->retry_count = 0;
        ctx->state       = USB_ENUM_STATE_GET_CONFIG_DESC_HEADER;
        break;

    case USB_ENUM_STATE_GET_CONFIG_DESC_HEADER:
        ret = usb_get_descriptor(ctx->device, USB_DT_CONFIG, 0, ctx->buffer,
                                 sizeof(usb_config_descriptor_t));
        if (ret != 0) {
            printk("USB: Failed to get config descriptor header\n");
            if (++ctx->retry_count < 3) { break; }
            ctx->state = USB_ENUM_STATE_ERROR;
            return -1;
        }

        usb_config_descriptor_t *config = (usb_config_descriptor_t *)ctx->buffer;

        ctx->retry_count = 0;
        ctx->state       = USB_ENUM_STATE_GET_CONFIG_DESC_FULL;
        break;

    case USB_ENUM_STATE_GET_CONFIG_DESC_FULL:
        printk("USB: Getting full configuration descriptor\n");

        {
            usb_config_descriptor_t *config       = (usb_config_descriptor_t *)ctx->buffer;
            uint16_t                 total_length = config->wTotalLength;

            if (total_length > ctx->buffer_size) {
                printk("USB: Config descriptor too large (%d > %d)\n", total_length,
                       ctx->buffer_size);
                ctx->state = USB_ENUM_STATE_ERROR;
                return -1;
            }

            ret = usb_get_descriptor(ctx->device, USB_DT_CONFIG, 0, ctx->buffer, total_length);
            if (ret != 0) {
                printk("USB: Failed to get full config descriptor\n");
                if (++ctx->retry_count < 3) { break; }
                ctx->state = USB_ENUM_STATE_ERROR;
                return -1;
            }

            printk("USB: Full configuration descriptor received (%d bytes)\n", total_length);

            // 解析配置描述符
            ret = usb_parse_config_descriptor(ctx->device, ctx->buffer, total_length);
            if (ret != 0) {
                printk("USB: Failed to parse configuration descriptor\n");
                ctx->state = USB_ENUM_STATE_ERROR;
                return -1;
            }
        }

        ctx->retry_count = 0;
        ctx->state       = USB_ENUM_STATE_SET_CONFIG;
        break;

    case USB_ENUM_STATE_SET_CONFIG:
        printk("USB: Setting configuration\n");

        {
            usb_config_descriptor_t *config = ctx->device->config_descriptor;
            if (!config) {
                printk("USB: No configuration descriptor available\n");
                ctx->state = USB_ENUM_STATE_ERROR;
                return -1;
            }

            ret = usb_set_configuration(ctx->device, config->bConfigurationValue);
            if (ret != 0) {
                printk("USB: Failed to set configuration\n");
                if (++ctx->retry_count < 3) { break; }
                ctx->state = USB_ENUM_STATE_ERROR;
                return -1;
            }
        }

        ctx->retry_count = 0;
        ctx->state       = USB_ENUM_STATE_CONFIGURE_ENDPOINTS;
        break;

    case USB_ENUM_STATE_CONFIGURE_ENDPOINTS:
        ret = usb_configure_device_endpoints(ctx->device);
        if (ret < 0) {
            printk("USB: Failed to configure endpoints\n");
            ctx->state = USB_ENUM_STATE_ERROR;
            return -1;
        }

        printk("USB: %d endpoints configured\n", ret);
        ctx->state = USB_ENUM_STATE_CONFIGURED;
        break;

    case USB_ENUM_STATE_CONFIGURED:
        printk("USB: Device enumeration complete!\n");

        // 添加设备到系统
        usb_add_device(ctx->device);

        ctx->device->state = USB_STATE_CONFIGURED;

        return 1; // 枚举完成

    case USB_ENUM_STATE_ERROR:
        printk("USB: Enumeration error in state machine\n");

        if (ctx->device) {
            printk("USB: Cleaning up failed enumeration\n");
            if (ctx->hcd->ops->disable_slot) { ctx->hcd->ops->disable_slot(ctx->hcd, ctx->device); }
            usb_free_device(ctx->device);
            ctx->device = NULL;
        }

        return -1;

    default:
        printk("USB: Unknown enumeration state: %d\n", ctx->state);
        ctx->state = USB_ENUM_STATE_ERROR;
        return -1;
    }

    return 0; // 继续枚举
}

int usb_enumerate_device(usb_hcd_t *hcd, uint8_t port_id, uint8_t speed) {
    printk("\n=== Starting USB Device Enumeration ===\n");
    printk("Port: %d, Speed: %d\n", port_id, speed);

    usb_enum_context_t *ctx = usb_alloc_enum_context(hcd, port_id);
    if (!ctx) {
        printk("USB: Failed to allocate enumeration context\n");
        return -1;
    }

    // 创建设备并设置速度
    ctx->device = usb_alloc_device(hcd);
    if (!ctx->device) {
        printk("USB: Failed to allocate device\n");
        usb_free_enum_context(ctx);
        return -1;
    }
    ctx->device->port  = port_id;
    ctx->device->speed = speed;

    // 运行状态机
    ctx->state = USB_ENUM_STATE_IDLE;

    int result;
    while (1) {
        result = usb_enum_state_machine(ctx);

        if (result == 1) {
            // 枚举成功
            printk("USB: Enumeration successful\n");
            break;
        } else if (result < 0) {
            // 枚举失败
            printk("USB: Enumeration failed\n");
            break;
        }
    }

    usb_free_enum_context(ctx);

    return result;
}