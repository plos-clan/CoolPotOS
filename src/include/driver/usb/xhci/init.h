#pragma once

#include "driver/pci/pci.h"

struct Xhci;

void xhci_init(pci_device_t *device);

extern struct Xhci *xhci_temp;
