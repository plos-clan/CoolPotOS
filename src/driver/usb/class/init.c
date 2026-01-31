#include "driver/usb/class/init.h"
#include "driver/usb/bus/driver.h"
#include "driver/usb/class/hid/kbd.h"
#include "driver/usb/class/hid/mouse.h"

void usb_class_init(void) {
    ProbeFnVec_push(&usb_drivers, hid_probe_kbd);
    ProbeFnVec_push(&usb_drivers, hid_probe_mouse);
}
