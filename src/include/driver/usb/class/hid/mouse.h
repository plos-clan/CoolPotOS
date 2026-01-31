#pragma once

#include "driver/usb/bus/driver.h"
#include "driver/usb/bus/iface.h"

UsbDriver *hid_probe_mouse(UsbInterface *iface);
