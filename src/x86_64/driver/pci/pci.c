#include "pci.h"
#include "acpi.h"
#include "alloc.h"
#include "hhdm.h"
#include "io.h"
#include "kprint.h"

uint32_t PCI_NUM = 0;

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

pci_device_t pci_device[PCI_DEVICE_MAX];
uint32_t     device_number = 0;

uint32_t get_pci_num() {
    return PCI_NUM;
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

static void pci_config0(uint32_t bus, uint32_t f, uint32_t equipment, uint32_t adder) {
    unsigned int cmd = 0;
    cmd = 0x80000000 + (uint32_t)adder + ((uint32_t)f << 8) + ((uint32_t)equipment << 11) +
          ((uint32_t)bus << 16);
    // cmd = cmd | 0x01;
    io_out32(PCI_COMMAND_PORT, cmd);
}

pci_device_t pci_find_vid_did(uint16_t vendor_id, uint16_t device_id) {
    for (size_t i = 0; i < device_number; i++) {
        if (pci_device[i]->vendor_id == vendor_id && pci_device[i]->device_id == device_id)
            return pci_device[i];
    }
    return NULL;
}

pci_device_t pci_find_class(uint32_t class_code) {
    for (size_t i = 0; i < device_number; i++) {
        if (pci_device[i]->class_code == class_code) { return pci_device[i]; }
        if (class_code == (pci_device[i]->class_code & 0xFFFF00)) { return pci_device[i]; }
    }
    return NULL;
}

static uint32_t read_pci0(uint32_t bus, uint32_t dev, uint32_t function, uint8_t registeroffset) {
    uint32_t id = 1U << 31 | ((bus & 0xff) << 16) | ((dev & 0x1f) << 11) |
                  ((function & 0x07) << 8) | (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    uint32_t result = io_in32(PCI_DATA_PORT);
    return result >> (8 * (registeroffset & 2) & 0xff);
}

static void write_pci0(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset,
                       uint32_t value) {
    uint32_t id = 1 << 31 | ((bus & 0xff) << 16) | ((device & 0x1f) << 11) |
                  ((function & 0x07) << 8) | (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    io_out32(PCI_DATA_PORT, value);
}

uint32_t read_pci(pci_device_t device, uint8_t registeroffset) {
    return read_pci0(device->bus, device->slot, device->func, registeroffset);
}

void write_pci(pci_device_t device, uint8_t offset, uint32_t value) {
    write_pci0(device->bus, device->slot, device->func, offset, value);
}

int pci_find_capability(pci_device_t device, uint8_t cap_id) {
    uint8_t pos = (uint8_t)(read_pci(device, PCI_CAPABILITY_LIST) & 0xFF);
    if (pos == 0) { return 0; }

    while (pos != 0) {
        uint8_t id = (uint8_t)(read_pci(device, pos + PCI_CAP_LIST_ID) & 0xFF);
        if (id == cap_id) { return pos; }
        pos = (uint8_t)(read_pci(device, pos + PCI_CAP_LIST_NEXT) & 0xFF);
    }

    return 0;
}

void pci_set_msi(pci_device_t device, uint8_t vector) {
    device->msi_offset = pci_find_capability(device, PCI_CAP_ID_MSI);
    uint32_t msg_ctrl  = read_pci(device, device->msi_offset + 2);
    uint8_t  reg0      = 0x4;
    uint8_t  reg1      = 0x8;

    if (((msg_ctrl >> 7) & 1) == 1) { reg1 = 0xc; }

    uint64_t address = (0xfee << 20) | (lapic_id() << 12);
    uint8_t  data    = vector;
    write_pci(device, device->msi_offset + reg0, address);
    write_pci(device, device->msi_offset + reg1, data);
    msg_ctrl |= 1;
    msg_ctrl &= ~(7 << 4);
    write_pci(device, device->msi_offset + 2, msg_ctrl);
}

static base_address_register get_base_address_register(uint8_t bus, uint8_t device,
                                                       uint8_t function, uint8_t bar) {
    base_address_register result = {0, NULL, 0, 0};

    uint32_t headertype = read_pci0(bus, device, function, 0x0e) & 0x7e;
    int      max_bars   = 6 - 4 * headertype;
    if (bar >= max_bars) return result;

    uint32_t bar_value = read_pci0(bus, device, function, 0x10 + 4 * bar);
    result.type        = (bar_value & 1) ? input_output : mem_mapping;

    if (result.type == mem_mapping) {
        switch ((bar_value >> 1) & 0x3) {
        case 0: // 32
        case 1: // 20
        case 2: // 64
            break;
        }
        result.address      = (uint8_t *)phys_to_virt(bar_value & ~0x3);
        result.prefetchable = 0;
    } else {
        result.address      = (uint8_t *)phys_to_virt(bar_value & ~0x3);
        result.prefetchable = 0;
    }
    return result;
}

base_address_register find_bar(pci_device_t device, uint8_t barNum) {
    base_address_register bar =
        get_base_address_register(device->bus, device->slot, device->func, barNum);
    return bar;
}

uint32_t read_bar_n(pci_device_t device, uint8_t bar_n) {
    uint32_t bar_offset = 0x10 + 4 * bar_n;
    return read_pci(device, bar_offset);
}

uint32_t pci_read_command_status(pci_device_t device) {
    return read_pci(device, 0x04);
}

void pci_write_command_status(pci_device_t device, uint32_t value) {
    write_pci0(device->bus, device->slot, device->func, 0x04, value);
}

bool pci_bar_present(pci_device_t device, uint8_t bar) {
    uint8_t reg_index = 0x10 + bar * 4;
    return read_pci(device, reg_index) != 0;
}

uint8_t pci_get_drive_irq(uint8_t bus, uint8_t slot, uint8_t func) {
    return (uint8_t)read_pci0(bus, slot, func, 0x3c);
}

static void load_pci_device(uint32_t BUS, uint32_t Equipment, uint32_t F) {
    uint32_t value_c    = read_pci0(BUS, Equipment, F, PCI_CONF_REVISION);
    uint32_t class_code = value_c >> 8;

    uint16_t value_v   = read_pci0(BUS, Equipment, F, PCI_CONF_VENDOR);
    uint16_t value_d   = read_pci0(BUS, Equipment, F, PCI_CONF_DEVICE);
    uint16_t vendor_id = value_v & 0xffff;
    uint16_t device_id = value_d & 0xffff;

    if (class_code == 0x060400 || (class_code & 0xFFFF00) == 0x060400) { return; }

    pci_device_t device = malloc(sizeof(struct pci_device));
    device->name        = pci_classname(class_code);
    device->vendor_id   = vendor_id;
    device->device_id   = device_id;
    device->class_code  = class_code;
    device->bus         = BUS;
    device->slot        = Equipment;
    device->func        = F;

    if (device_number > PCI_DEVICE_MAX) {
        kwarn("add device full %d", device_number);
        return;
    }
    pci_device[device_number++] = device;
}

void print_all_pci() {
    printk("Bus:Slot:Func\t[Vendor:Device]\tClass Code\tName\n");
    for (size_t i = 0; i < device_number; i++) {
        pci_device_t device = pci_device[i];
        printk("%03d:%02d:%02d\t[0x%04X:0x%04X]\t<0x%08x>\t%s\n", device->bus, device->slot,
               device->func, device->vendor_id, device->device_id, device->class_code,
               device->name);
    }
}

void pci_setup() {
    uint32_t BUS, Equipment, F;
    for (BUS = 0; BUS < 256; BUS++) {
        for (Equipment = 0; Equipment < 32; Equipment++) {
            for (F = 0; F < 8; F++) {
                pci_config0(BUS, F, Equipment, 0);
                if (io_in32(PCI_DATA_PORT) != 0xFFFFFFFF) {
                    PCI_NUM++;
                    load_pci_device(BUS, Equipment, F);
                }
            }
        }
    }
    kinfo("PCI device loaded: %d", PCI_NUM);
}
