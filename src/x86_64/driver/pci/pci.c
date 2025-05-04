#include "pci.h"
#include "acpi.h"
#include "heap.h"
#include "hhdm.h"
#include "io.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"

struct {
    uint32_t classcode;
    char    *name;
} pci_classnames[] = {
    {0x000000, "Non-VGA-Compatible Unclassified Device"     },
    {0x000100, "VGA-Compatible Unclassified Device"         },

    {0x010000, "SCSI Bus Controller"                        },
    {0x010100, "IDE Controller"                             },
    {0x010200, "Floppy Disk Controller"                     },
    {0x010300, "IPI Bus Controller"                         },
    {0x010400, "RAID Controller"                            },
    {0x010500, "ATA Controller"                             },
    {0x010600, "Serial ATA Controller"                      },
    {0x010700, "Serial Attached SCSI Controller"            },
    {0x010800, "Non-Volatile Memory Controller"             },
    {0x018000, "Other Mass Storage Controller"              },

    {0x020000, "Ethernet Controller"                        },
    {0x020100, "Token Ring Controller"                      },
    {0x020200, "FDDI Controller"                            },
    {0x020300, "ATM Controller"                             },
    {0x020400, "ISDN Controller"                            },
    {0x020500, "WorldFip Controller"                        },
    {0x020600, "PICMG 2.14 Multi Computing Controller"      },
    {0x020700, "Infiniband Controller"                      },
    {0x020800, "Fabric Controller"                          },
    {0x028000, "Other Network Controller"                   },

    {0x030000, "VGA Compatible Controller"                  },
    {0x030100, "XGA Controller"                             },
    {0x030200, "3D Controller (Not VGA-Compatible)"         },
    {0x038000, "Other Display Controller"                   },

    {0x040000, "Multimedia Video Controller"                },
    {0x040100, "Multimedia Audio Controller"                },
    {0x040200, "Computer Telephony Device"                  },
    {0x040300, "Audio Device"                               },
    {0x048000, "Other Multimedia Controller"                },

    {0x050000, "RAM Controller"                             },
    {0x050100, "Flash Controller"                           },
    {0x058000, "Other Memory Controller"                    },

    {0x060000, "Host Bridge"                                },
    {0x060100, "ISA Bridge"                                 },
    {0x060200, "EISA Bridge"                                },
    {0x060300, "MCA Bridge"                                 },
    {0x060400, "PCI-to-PCI Bridge"                          },
    {0x060500, "PCMCIA Bridge"                              },
    {0x060600, "NuBus Bridge"                               },
    {0x060700, "CardBus Bridge"                             },
    {0x060800, "RACEway Bridge"                             },
    {0x060900, "PCI-to-PCI Bridge"                          },
    {0x060A00, "InfiniBand-to-PCI Host Bridge"              },
    {0x068000, "Other Bridge"                               },

    {0x070000, "Serial Controller"                          },
    {0x070100, "Parallel Controller"                        },
    {0x070200, "Multiport Serial Controller"                },
    {0x070300, "Modem"                                      },
    {0x070400, "IEEE 488.1/2 (GPIB) Controller"             },
    {0x070500, "Smart Card Controller"                      },
    {0x078000, "Other Simple Communication Controller"      },

    {0x080000, "PIC"                                        },
    {0x080100, "DMA Controller"                             },
    {0x080200, "Timer"                                      },
    {0x080300, "RTC Controller"                             },
    {0x080400, "PCI Hot-Plug Controller"                    },
    {0x080500, "SD Host controller"                         },
    {0x080600, "IOMMU"                                      },
    {0x088000, "Other Base System Peripheral"               },

    {0x090000, "Keyboard Controller"                        },
    {0x090100, "Digitizer Pen"                              },
    {0x090200, "Mouse Controller"                           },
    {0x090300, "Scanner Controller"                         },
    {0x090400, "Gameport Controller"                        },
    {0x098000, "Other Input Device Controller"              },

    {0x0A0000, "Generic"                                    },
    {0x0A8000, "Other Docking Station"                      },

    {0x0B0000, "386"                                        },
    {0x0B0100, "486"                                        },
    {0x0B0200, "Pentium"                                    },
    {0x0B0300, "Pentium Pro"                                },
    {0x0B1000, "Alpha"                                      },
    {0x0B2000, "PowerPC"                                    },
    {0x0B3000, "MIPS"                                       },
    {0x0B4000, "Co-Processor"                               },
    {0x0B8000, "Other Processor"                            },

    {0x0C0000, "FireWire (IEEE 1394) Controller"            },
    {0x0C0100, "ACCESS Bus Controller"                      },
    {0x0C0200, "SSA"                                        },
    {0x0C0300, "USB Controller"                             },
    {0x0C0400, "Fibre Channel"                              },
    {0x0C0500, "SMBus Controller"                           },
    {0x0C0600, "InfiniBand Controller"                      },
    {0x0C0700, "IPMI Interface"                             },
    {0x0C0800, "SERCOS Interface (IEC 61491)"               },
    {0x0C0900, "CANbus Controller"                          },
    {0x0C8000, "Other Serial Bus Controller"                },

    {0x0D0000, "iRDA Compatible Controlle"                  },
    {0x0D0100, "Consumer IR Controller"                     },
    {0x0D1000, "RF Controller"                              },
    {0x0D1100, "Bluetooth Controller"                       },
    {0x0D1200, "Broadband Controller"                       },
    {0x0D2000, "Ethernet Controller (802.1a)"               },
    {0x0D2100, "Ethernet Controller (802.1b)"               },
    {0x0D8000, "Other Wireless Controller"                  },

    {0x0E0000, "I20"                                        },

    {0x0F0000, "Satellite TV Controller"                    },
    {0x0F0100, "Satellite Audio Controller"                 },
    {0x0F0300, "Satellite Voice Controller"                 },
    {0x0F0400, "Satellite Data Controller"                  },

    {0x100000, "Network and Computing Encrpytion/Decryption"},
    {0x101000, "Entertainment Encryption/Decryption"        },
    {0x108000, "Other Encryption Controller"                },

    {0x110000, "DPIO Modules"                               },
    {0x110100, "Performance Counters"                       },
    {0x111000, "Communication Synchronizer"                 },
    {0x112000, "Signal Processing Management"               },
    {0x118000, "Other Signal Processing Controller"         },
    {0x000000, NULL                                         }
};

MCFG_ENTRY *mcfg_entries[PCI_MCFG_MAX_ENTRIES_LEN];
MCFG       *mcfg             = NULL;
uint64_t    mcfg_entries_len = 0;

pci_device_t *pci_devices[PCI_DEVICE_MAX];
uint32_t      device_number = 0;

uint32_t get_pci_num() {
    return device_number;
}

static uint64_t get_device_mmio_physical_address(uint16_t segment_group, uint8_t bus,
                                                 uint8_t device, uint8_t function) {
    for (size_t i = 0; i < mcfg_entries_len; i++) {
        if (mcfg_entries[i]->pci_segment_group == segment_group) {
            return mcfg_entries[i]->base_address + ((bus - mcfg_entries[i]->start_bus) << 20) +
                   (device << 15) + (function << 12);
        }
    }
    return 0;
}

static uint64_t get_mmio_address(uint32_t pci_address, uint16_t offset) {
    uint16_t segment  = (pci_address >> 16) & 0xFFFF;
    uint8_t  bus      = (pci_address >> 8) & 0xFF;
    uint8_t  device   = (pci_address >> 3) & 0x1F;
    uint8_t  function = pci_address & 0x07;
    return (uint64_t)(((uint64_t)phys_to_virt(
                          get_device_mmio_physical_address(segment, bus, device, function))) +
                      offset);
}

static uint32_t segment_bus_device_functon_to_pci_address(uint16_t segment, uint8_t bus,
                                                          uint8_t device, uint8_t function) {
    return ((uint32_t)segment << 16) | ((uint32_t)bus << 8) | ((uint32_t)device << 3) |
           (uint32_t)function;
}

static void pci_config0(uint32_t bus, uint32_t f, uint32_t equipment, uint32_t adder) {
    unsigned int cmd = 0;
    cmd = 0x80000000 + (uint32_t)adder + ((uint32_t)f << 8) + ((uint32_t)equipment << 11) +
          ((uint32_t)bus << 16);
    io_out32(PCI_COMMAND_PORT, cmd);
}

void mcfg_addr_to_entries(MCFG_ENTRY **entries) {
    MCFG_ENTRY *entry  = (MCFG_ENTRY *)((uint64_t)mcfg + sizeof(MCFG));
    uint64_t    length = mcfg->Header.Length - sizeof(MCFG);
    mcfg_entries_len   = length / sizeof(MCFG_ENTRY);
    for (size_t i = 0; i < mcfg_entries_len; i++) {
        *(entries + i) = entry + i;
    }
}

char *pci_classname(uint32_t classcode) {
    for (size_t i = 0; pci_classnames[i].name != NULL; i++) {
        if (pci_classnames[i].classcode == classcode) { return pci_classnames[i].name; }
        if (pci_classnames[i].classcode == (classcode & 0xFFFF00)) {
            return pci_classnames[i].name;
        }
    }
    return "Unknown device";
}

uint32_t pci_read0(uint32_t b, uint32_t d, uint32_t f, uint32_t arg, uint32_t registeroffset) {
    uint32_t id = (1U << 31) | ((b & 0xff) << 16) | ((d & 0x1f) << 11) | ((f & 0x07) << 8) |
                  (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    uint32_t result = io_in32(PCI_DATA_PORT);
    return result >> ((8 * (registeroffset & 2)) & 0xFF);
}

void pci_write0(uint32_t b, uint32_t d, uint32_t f, uint32_t arg, uint32_t registeroffset,
                uint32_t value) {
    uint32_t id = (1U << 31) | ((b & 0xff) << 16) | ((d & 0x1f) << 11) | ((f & 0x07) << 8) |
                  (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    io_out32(PCI_DATA_PORT, value);
}

pci_device_op_t pci_device_op = {
    .read  = pci_read0,
    .write = pci_write0,
};

uint32_t pci_read(uint32_t b, uint32_t d, uint32_t f, uint32_t s, uint32_t offset) {
    uint32_t pci_address  = segment_bus_device_functon_to_pci_address(s, b, d, f);
    uint64_t mmio_address = get_mmio_address(pci_address, offset);
    if (mmio_address == 0) { kerror("Cannot read pci: failed to get mmio address"); }
    return *(uint32_t *)mmio_address;
}

void pci_write(uint32_t b, uint32_t d, uint32_t f, uint32_t s, uint32_t offset, uint32_t value) {
    uint32_t pci_address  = segment_bus_device_functon_to_pci_address(s, b, d, f);
    uint64_t mmio_address = get_mmio_address(pci_address, offset);
    if (mmio_address == 0) { kerror("Cannot write pci: failed to get mmio address"); }
    *(uint32_t *)mmio_address = value;
}

uint32_t pci_read_command(pci_device_t *device, uint8_t offset) {
    uint32_t address = (0x80000000) | (device->bus << 16) | (device->slot << 11) |
                       (device->func << 8) | (offset & 0xFC);
    io_out32(PCI_COMMAND_PORT, address);
    return io_in32(PCI_DATA_PORT);
}

void pci_write_command(pci_device_t *device, uint8_t offset, uint32_t value) {
    uint32_t address = (0x80000000) | (device->bus << 16) | (device->slot << 11) |
                       (device->func << 8) | (offset & 0xFC);
    io_out32(PCI_COMMAND_PORT, address);
    io_out32(PCI_DATA_PORT, value);
}

uint32_t pci_enumerate_capability_list(pci_device_t *pci_dev, uint32_t cap_type) {
    uint32_t cap_offset;
    switch (pci_dev->header_type) {
    case 0x00: cap_offset = pci_dev->capability_point; break;
    case 0x10: cap_offset = pci_dev->capability_point; break;
    default:
        // 不支持
        return 0;
    }

    uint32_t tmp;
    while (1) {
        tmp = pci_dev->op->read(pci_dev->bus, pci_dev->slot, pci_dev->func, pci_dev->segment,
                                cap_offset);
        if ((tmp & 0xff) != cap_type) {
            if (((tmp & 0xff00) >> 8)) {
                cap_offset = (tmp & 0xff00) >> 8;
                continue;
            } else
                return 0;
        }

        return cap_offset;
    }
}

pci_device_op_t pcie_device_op = {
    .read  = pci_read,
    .write = pci_write,
};

void pci_scan_function(uint16_t segment_group, uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t pci_address =
        segment_bus_device_functon_to_pci_address(segment_group, bus, device, function);

    uint64_t id_mmio_addr = get_mmio_address(pci_address, 0x00);
    uint16_t vendor_id    = *(uint16_t *)id_mmio_addr;
    if (vendor_id == 0xFFFF) { return; }
    uint16_t device_id = *(uint16_t *)(id_mmio_addr + 2);

    uint64_t field_mmio_addr  = get_mmio_address(pci_address, 0x08);
    uint8_t  device_class     = *((uint8_t *)field_mmio_addr + 3);
    uint8_t  device_subclass  = *((uint8_t *)field_mmio_addr + 2);
    uint8_t  device_interface = *((uint8_t *)field_mmio_addr + 1);

    uint64_t header_type_mmio_addr = get_mmio_address(pci_address, 0x0c);
    uint8_t  header_type           = (*((uint8_t *)header_type_mmio_addr + 2)) & 0x7F;

    pci_device_t *pci_device = (pci_device_t *)malloc(sizeof(pci_device_t));
    memset(pci_device, 0, sizeof(pci_device_t));
    pci_device->header_type = header_type;
    pci_device->op          = &pcie_device_op;

    pci_device->segment = segment_group;
    pci_device->bus     = bus;
    pci_device->slot    = device;
    pci_device->func    = function;

    switch (header_type) {
    // Endpoint
    case 0x00: {
        uint32_t class_code_24bit =
            (device_class << 16) | (device_subclass << 8) | device_interface;
        pci_device->class_code = class_code_24bit;
        pci_device->name       = pci_classname(class_code_24bit);
        pci_device->vendor_id  = vendor_id;
        pci_device->device_id  = device_id;

        logkf("Found PCIe device: %#08lx name: %s\n", pci_device->class_code, pci_device->name);

        uint32_t capability_point =
            pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                 pci_device->segment, 0x34) &
            0xff;
        pci_device->capability_point = capability_point;

        for (int i = 0; i < 6; i++) {
            int      offset            = 0x10 + i * 4;
            uint64_t bars_mmio_address = get_mmio_address(pci_address, offset);
            uint32_t bar               = *(uint32_t *)bars_mmio_address;

            if (bar & 0x01) {
                pci_device->bars[i].address = bar & 0xFFFFFFFC;
                pci_device->bars[i].size    = 0;
                pci_device->bars[i].mmio    = false;
            } else {
                uint64_t bar_address  = bar & 0xFFFFFFF0;

                switch ((bar & ((1 << 3) | (1 << 2) | (1 << 1))) >> 1) {
                // 32 bit
                case 0b00: {
                    pci_device->bars[i].address = bar & 0xFFFFFFFC;
                    pci_device->bars[i].mmio    = false;

                    uint32_t original_value =
                        pci_device->op->read(bus, device, function, segment_group, offset);

                    pci_device->op->write(bus, device, function, segment_group, offset, 0xFFFFFFFF);
                    uint32_t value =
                        pci_device->op->read(bus, device, function, segment_group, offset);

                    pci_device->op->write(bus, device, function, segment_group, offset,
                                          original_value);

                    uint32_t mask = (uint32_t)(value & 0xFFFFFFF0);

                    pci_device->bars[i].size = (uint64_t)(~mask + 1);
                } break;

                    // 64 bit
                case 0b10: {
                }
                    uint32_t bar_address_upper = *((uint32_t *)bars_mmio_address + 1);

                    bar_address |= ((uint64_t)bar_address_upper << 32);

                    uint32_t original_value =
                        pci_device->op->read(bus, device, function, segment_group, offset);
                    uint32_t original_value_high =
                        pci_device->op->read(bus, device, function, segment_group, offset + 4);

                    pci_device->op->write(bus, device, function, segment_group, offset, 0xFFFFFFFF);
                    pci_device->op->write(bus, device, function, segment_group, offset + 4,
                                          0xFFFFFFFF);
                    uint32_t mask =
                        pci_device->op->read(bus, device, function, segment_group, offset);
                    uint32_t mask_high =
                        pci_device->op->read(bus, device, function, segment_group, offset + 4);

                    pci_device->op->write(bus, device, function, segment_group, offset,
                                          original_value);
                    pci_device->op->write(bus, device, function, segment_group, offset + 4,
                                          original_value_high);

                    uint64_t value = ((uint64_t)mask_high << 32) | (mask & 0xFFFFFFF0);

                    pci_device->bars[i].size = ~value + 1;

                    pci_device->bars[i].address = bar_address;
                    pci_device->bars[i].mmio    = false;
                    break;
                }
            }
        }

        pci_devices[device_number] = pci_device;
        device_number++;

        break;
    }
        // PciPciBridge
    case 0x01: {
        uint32_t data = pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, 0x18);
        uint8_t  start_bus = (uint8_t)((data >> 8) & 0xFF);
        uint8_t  end_bus   = (uint8_t)((data >> 16) & 0xFF);
        for (uint8_t bus_index = start_bus; bus_index <= end_bus; bus_index++) {
            pci_scan_bus(segment_group, bus_index);
        }

        free(pci_device);
        break;
    }
        // CardBusBridge
    case 0x02:
        // Ignore
        break;
    default: kerror("Failed to parse header type, header type = %#04x", header_type);
    }
}

void pci_scan_device_legacy(uint32_t bus, uint32_t equipment, uint32_t f) {
    pci_device_t *device = (pci_device_t *)malloc(sizeof(pci_device_t));
    memset(device, 0, sizeof(pci_device_t));
    device->op = &pci_device_op;

    uint32_t value_c    = device->op->read(bus, equipment, f, 0, PCI_CONF_REVISION);
    uint32_t class_code = value_c >> 8;

    uint16_t value_v   = device->op->read(bus, equipment, f, 0, PCI_CONF_VENDOR);
    uint16_t value_d   = device->op->read(bus, equipment, f, 0, PCI_CONF_DEVICE);
    uint16_t vendor_id = value_v & 0xffff;
    uint16_t device_id = value_d & 0xffff;

    device->name       = pci_classname(class_code);
    device->vendor_id  = vendor_id;
    device->device_id  = device_id;
    device->class_code = class_code;
    device->segment    = 0;
    device->bus        = bus;
    device->slot       = equipment;
    device->func       = f;

    logkf("Found PCI device: %#08lx name: %s\n", device->class_code, device->name);

    for (int i = 0; i < 6; i++) {
        int      offset  = 0x10 + i * 4;
        uint32_t bar_low = device->op->read(bus, equipment, f, 0, offset);

        device->bars[i].mmio    = false;
        device->bars[i].address = 0;
        device->bars[i].size    = 0;

        if (bar_low & 0x1) {
            device->bars[i].mmio    = false;
            device->bars[i].address = bar_low & 0xFFFFFFFC;
        } else {
            device->bars[i].mmio = true;
            uint8_t bar_type     = (bar_low >> 1) & 0x3;

            if (bar_type == 0x0) {
                device->bars[i].address = bar_low & 0xFFFFFFF0;

                uint32_t original_value = device->op->read(bus, equipment, f, 0, offset);

                device->op->write(bus, equipment, f, 0, offset, 0xFFFFFFFF);
                uint32_t value = device->op->read(bus, equipment, f, 0, offset);

                device->op->write(bus, equipment, f, 0, offset, original_value);

                uint32_t mask = (uint32_t)(value & 0xFFFFFFF0);

                device->bars[i].size = (uint64_t)(~mask + 1);
            } else if (bar_type == 0x2) {
                if (i >= 5) {
                    kerror("Error: 64-bit BAR at position overflow");
                    continue;
                }

                uint32_t bar_high       = device->op->read(bus, equipment, f, 0, offset + 4);
                device->bars[i].address = ((uint64_t)bar_high << 32) | (bar_low & 0xFFFFFFF0);

                uint32_t original_value      = device->op->read(bus, equipment, f, 0, offset);
                uint32_t original_value_high = device->op->read(bus, equipment, f, 0, offset + 4);

                device->op->write(bus, equipment, f, 0, offset, 0xFFFFFFFF);
                device->op->write(bus, equipment, f, 0, offset + 4, 0xFFFFFFFF);
                uint32_t mask      = device->op->read(bus, equipment, f, 0, offset);
                uint32_t mask_high = device->op->read(bus, equipment, f, 0, offset + 4);

                device->op->write(bus, equipment, f, 0, offset, original_value);
                device->op->write(bus, equipment, f, 0, offset + 4, original_value_high);

                uint64_t mask_value = ((uint64_t)mask_high << 32) | (mask & 0xFFFFFFF0);

                device->bars[i].size = ~mask_value + 1;

                i++;

                device->bars[i].mmio    = true;
                device->bars[i].address = 0;
                device->bars[i].size    = 0;
            }
        }
    }

    pci_devices[device_number] = device;
    device_number++;
}

void pci_scan_bus(uint16_t segment_group, uint8_t bus) {
    for (int i = 0; i < 32; i++) {
        pci_scan_function(segment_group, bus, i, 0);
        uint32_t pci_address = segment_bus_device_functon_to_pci_address(segment_group, bus, i, 0);
        uint64_t mmio_addr   = get_mmio_address(pci_address, 0x0c);
        if (*(uint32_t *)mmio_addr & (1UL << 23)) {
            for (int j = 1; j < 8; j++) {
                pci_scan_function(segment_group, bus, i, j);
            }
        }
    }
}

void pci_scan_segment(uint16_t segment_group) {
    pci_scan_bus(segment_group, 0);
    uint32_t pci_address = segment_bus_device_functon_to_pci_address(segment_group, 0, 0, 0);
    uint64_t mmio_addr   = get_mmio_address(pci_address, 0x0c);
    if (*(uint32_t *)mmio_addr & (1 << 23)) {
        for (int i = 1; i < 8; i++) {
            pci_scan_bus(segment_group, i);
        }
    }
}

pci_device_t *pci_find_vid_did(uint16_t vendor_id, uint16_t device_id) {
    for (size_t i = 0; i < device_number; i++) {
        if (pci_devices[i]->vendor_id == vendor_id && pci_devices[i]->device_id == device_id)
            return pci_devices[i];
    }
    return NULL;
}

pci_device_t *pci_find_class(uint32_t class_code) {
    for (size_t i = 0; i < device_number; i++) {
        if (pci_devices[i]->class_code == class_code) { return pci_devices[i]; }
        if (class_code == (pci_devices[i]->class_code & 0xFFFF00)) { return pci_devices[i]; }
    }
    return NULL;
}

void pci_setup(MCFG *mcfg0) {
    mcfg = mcfg0;
}

void pci_init() {
    if (mcfg) {
        mcfg_addr_to_entries(mcfg_entries);
        for (size_t i = 0; i < mcfg_entries_len; i++) {
            uint16_t segment_group = mcfg_entries[i]->pci_segment_group;
            pci_scan_segment(segment_group);
        }
    } else {
        uint32_t BUS, Equipment, F;
        for (BUS = 0; BUS < 256; BUS++) {
            for (Equipment = 0; Equipment < 32; Equipment++) {
                for (F = 0; F < 8; F++) {
                    pci_config0(BUS, F, Equipment, 0);
                    if (io_in32(PCI_DATA_PORT) != 0xFFFFFFFF) {
                        pci_scan_device_legacy(BUS, Equipment, F);
                    }
                }
            }
        }
    }
    kinfo("%s device loaded: %d", mcfg == NULL ? "PCI " : "PCIE", device_number);
}
