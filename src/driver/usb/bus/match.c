#include "driver/usb/bus/device.h"
#include "driver/usb/bus/driver.h"
#include "term/klog.h"

void usb_device_match_drivers(UsbDevice *dev) {
    if (usb_drivers.len == 0) {
        kwarn("No USB drivers registered");
        return;
    }

    for (size_t i = 0; i < dev->interfaces.len; i++) {
        UsbInterface *iface = &dev->interfaces.data[i];
        if (iface->driver) {
            continue;
        }

        for (size_t j = 0; j < usb_drivers.len; j++) {
            ProbeFn probe_fn  = usb_drivers.data[j];
            UsbDriver *driver = probe_fn(iface);
            if (driver) {
                iface->driver = driver;
                kinfo("Interface bound to driver successfully");
                break;
            }
        }
    }
}
