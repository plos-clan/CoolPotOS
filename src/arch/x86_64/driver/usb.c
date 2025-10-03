#include "usb.h"
#include "heap.h"
#include "kprint.h"
#include "xhci.h"

#define klog(...)                                                                                  \
    do {                                                                                           \
        kinfo(__VA_ARGS__);                                                                        \
    } while (0);

#define error(...)                                                                                 \
    do {                                                                                           \
        kerror(__VA_ARGS__);                                                                       \
    } while (0);

USB_CONTROLLER *USB_CTRL       = 0;
USB_COMMON     *ALL_USB_DEVICE = 0;

const uint32_t SPEED_TO_CTRL_SIZE[] = {
    8,   // FULL  SPEED
    8,   // LOW   SPEED
    64,  // HIGH  SPEED
    512, // SUPER SPEED
};

uint32_t USBEnumerate(USB_HUB *hub) {
    for (uint32_t port = 0; port < hub->PC; port++) {
        USB_COMMON *common;
        uint32_t    count;

        if (!hub->OP->DTC(hub, port)) {
            // No device found.
            goto NEXT_PORT;
        }
        common = (USB_COMMON *)malloc(sizeof(USB_COMMON));
        memset(common, 0, sizeof(USB_COMMON));
        common->CTRL = hub->CTRL;
        common->HUB  = hub;
        common->PORT = port;

        // Reset port and determine device speed
        common->SPD = hub->OP->RST(hub, port);
        if (common->SPD < 0) { goto RESET_FAILED; }

        // Set address of port
        if (USBSetAddress(common)) {
            hub->OP->DCC(hub, port);
            goto RESET_FAILED;
        }

        // Configure device
        count = ConfigureUSB(common);
        if (!count) {
            hub->OP->DCC(hub, port);
            free(common);
            continue;
        }
        hub->DC    += count;
        common->A1  = ALL_USB_DEVICE;
        if (ALL_USB_DEVICE) { ALL_USB_DEVICE->A0 = common; }
        ALL_USB_DEVICE = common;

        continue;

    RESET_FAILED:;
        free(common);

    NEXT_PORT:;
    }
    return hub->DC;
}
uint32_t USBSetAddress(USB_COMMON *device) {
    USB_CONTROLLER *controller = device->CTRL;
    if (controller->MA >= USB_MAXADDR) return 1;

    // Create a pipe for the default address
    USB_ENDPOINT epdesc;
    memset(&epdesc, 0, sizeof(USB_ENDPOINT));
    epdesc.MPS   = SPEED_TO_CTRL_SIZE[device->SPD];
    epdesc.AT    = USB_ENDPOINT_XFER_CONTROL;
    device->PIPE = controller->CPIP(device, 0, &epdesc);
    if (!device->PIPE) { return -1; }

    // Send SET_ADDRESS command.
    USB_DEVICE_REQUEST req;
    req.T = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
    req.C = USB_REQ_SET_ADDRESS;
    req.V = controller->MA = 1;
    req.I                  = 0;
    req.L                  = 0;
    uint32_t cc            = controller->XFER(device->PIPE, &req, 0, 0, 0);
    if (cc) {
        error("CANNOT SET ADDRESS\n");
        // Free pipe
        return -1;
    }

    controller->MA++;
    device->PIPE = controller->CPIP(device, device->PIPE, &epdesc);
    if (!device->PIPE) { return -1; }
    return 0;
}
uint32_t ConfigureUSB(USB_COMMON *common) {
    USB_CONTROLLER *controller = common->CTRL;
    // Set the max packet size for endpoint 0 of this device
    // Get first 8 byte of device desc
    uint64_t        devinfo_phys = (uint64_t)alloc_frames(1);
    USB_DEVICE     *devinfo      = (USB_DEVICE *)phys_to_virt(devinfo_phys);

    memset(devinfo, 0, sizeof(USB_DEVICE));

    USB_DEVICE_REQUEST req;
    req.T       = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
    req.C       = USB_REQ_GET_DESCRIPTOR;
    req.V       = USB_DT_DEVICE << 8;
    req.I       = 0;
    req.L       = 8;
    uint32_t cc = controller->XFER(common->PIPE, &req, devinfo, 0, 0);
    if (cc) {
        error("TRANSFER FAILED\n");
        return 0;
    }

    uint16_t maxpacket = devinfo->MPS;
    if (devinfo->VER >= 0x0300) maxpacket = 1 << devinfo->MPS;
    if (maxpacket < 8) return 0;

    USB_ENDPOINT epdesc = {
        .L   = 0,
        .DT  = 0,
        .EA  = 0,
        .AT  = USB_ENDPOINT_XFER_CONTROL,
        .MPS = maxpacket,
        .ITV = 0,
    };

    common->PIPE = controller->CPIP(common, common->PIPE, &epdesc);
    free_frame((uint64_t)virt_to_phys((uint64_t)devinfo));

    // Get device config
    USB_CONFIG *config = 0;

    {
        uint64_t    cfg_phys = (uint64_t)alloc_frames(1);
        USB_CONFIG *cfg      = (USB_CONFIG *)phys_to_virt(cfg_phys);

        memset(cfg, 0, sizeof(USB_CONFIG));
        req.T = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
        req.C = USB_REQ_GET_DESCRIPTOR;
        req.V = USB_DT_CONFIG << 8;
        req.I = 0;
        req.L = sizeof(USB_CONFIG);

        if (controller->XFER(common->PIPE, &req, cfg, 0, 0)) { goto LABEL001; }

        config = (USB_CONFIG *)malloc(cfg->TL);
        memset(config, 0, cfg->TL);

        req.L = cfg->TL;
        if (controller->XFER(common->PIPE, &req, config, 0, 0) || config->TL != cfg->TL) {
            free(config);
            config = 0;
        }
    }

LABEL001:;
    if (!config) {
        error("CANNOT GET CONFIG\n");
        return 0;
    }

    uint64_t ic    = config->IC;
    uint64_t iface = ((uint64_t)config) + 9;
    uint64_t cfend = ((uint64_t)config) + config->TL;

    loop {
        USB_INTERFACE *interface = (USB_INTERFACE *)iface;
        if ((!ic) || ((iface + interface->L) > cfend)) goto CONFIG_FAIL; // goto FAILED;

        if (interface->DT == USB_DT_INTERFACE) {
            ic--;
            break;
        }
        iface += interface->L;
    }

    USB_INTERFACE *interface;

    req.T = USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
    req.C = USB_REQ_SET_CONFIGURATION;
    req.V = config->CV;
    req.I = 0;
    req.L = 0;
    cc    = controller->XFER(common->PIPE, &req, 0, 0, 0);
    if (cc) {
        error("CANNOT SET CONFIGURATION\n");
        goto CONFIG_FAIL;
    }

    interface   = (USB_INTERFACE *)iface;
    common->CFG = config;
    common->IFC = interface;
    cc          = -1;
    if (interface->DT == USB_DT_INTERFACE) {
        if (interface->IC == USB_CLASS_HUB) {
            klog("HUB device\n");
            // cc = ConfigureHUB(common);
        } else if (interface->IC == USB_CLASS_MASS_STORAGE) {
            if (interface->IP == US_PR_BULK) {
                klog("MASS device\n");
                // cc = ConfigureMSC(common, interface);
            }
        } else if (interface->IC == USB_CLASS_HID) {
            klog("HID device\n");
            // cc = ConfigureHID(common);
        }
    }
    if (cc) {
        // write_serial_string("CANNOT CONFIGURE DEVICE\n");
        goto CONFIG_FAIL;
    }

    return 1;

CONFIG_FAIL:;
    free(config);
    common->CFG = 0;
    common->IFC = 0;
    return 0;
}
void USBD2P(USB_PIPE *pipe, USB_COMMON *usbdev, USB_ENDPOINT *epdesc) {
    pipe->CTRL = usbdev->CTRL;
    pipe->MPS  = epdesc->MPS;
}
USB_ENDPOINT *USBSearchEndpoint(USB_COMMON *usbdev, int type, int dir) {
    USB_CONFIG    *config = usbdev->CFG;
    USB_INTERFACE *iface  = usbdev->IFC;
    uint64_t       imax   = ((uint64_t)config) + config->TL - ((uint64_t)iface);
    USB_ENDPOINT  *epdesc = (USB_ENDPOINT *)(((uint64_t)iface) + 9);
    loop {
        if ((((uint64_t)epdesc) >= ((uint64_t)iface) + imax) || (epdesc->DT == USB_DT_INTERFACE))
            return 0;
        uint32_t edir = epdesc->EA & USB_ENDPOINT_DIR;
        uint32_t etyp = epdesc->AT & USB_ENDPOINT_XFER_TYPE;
        if ((epdesc->DT == USB_DT_ENDPOINT) && (edir == (uint32_t)dir) && (etyp == (uint32_t)type))
            return epdesc;
        epdesc = (USB_ENDPOINT *)(((uint64_t)epdesc) + epdesc->L);
    }
    return 0;
}
