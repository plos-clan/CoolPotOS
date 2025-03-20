#include "pci.h"
#include "io.h"
#include "klog.h"
#include "kmalloc.h"
#include "krlibc.h"

unsigned int PCI_ADDR_BASE;
unsigned int PCI_NUM = 0;

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

pci_device_t *pci_device[PCI_DEVICE_MAX];
uint32_t      device_number = 0;

uint32_t get_pci_num() {
    return PCI_NUM;
}

uint8_t pci_get_drive_irq(uint8_t bus, uint8_t slot, uint8_t func) {
    return (uint8_t)read_pci(bus, slot, func, 0x3c);
}

uint32_t pci_get_port_base(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t io_port = 0;
    for (int i = 0; i < 6; i++) {
        base_address_register bar = get_base_address_register(bus, slot, func, i);
        if (bar.type == input_output) { io_port = (uint32_t)bar.address; }
    }
    return io_port;
}

void write_pci(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset,
               uint32_t value) {
    uint32_t id = 1 << 31 | ((bus & 0xff) << 16) | ((device & 0x1f) << 11) |
                  ((function & 0x07) << 8) | (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    io_out32(PCI_DATA_PORT, value);
}

uint32_t pci_read_command_status(uint8_t bus, uint8_t slot, uint8_t func) {
    return read_pci(bus, slot, func, 0x04);
}

void pci_write_command_status(uint8_t bus, uint8_t slot, uint8_t func, uint32_t value) {
    write_pci(bus, slot, func, 0x04, value);
}

uint32_t pci_dev_read32(pci_device_t *pdev, uint16_t offset) {
    return read_pci(pdev->bus, pdev->slot, pdev->func, offset);
}

uint32_t read_pci(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset) {
    uint32_t id = 1U << 31 | ((bus & 0xff) << 16) | ((device & 0x1f) << 11) |
                  ((function & 0x07) << 8) | (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    uint32_t result = io_in32(PCI_DATA_PORT);
    return result >> (8 * (registeroffset % 4));
}

uint32_t read_bar_n(pci_device_t *device, uint8_t bar_n) {
    uint32_t bar_offset = 0x10 + 4 * bar_n;
    return read_pci(device->bus, device->slot, device->func, bar_offset);
}

base_address_register get_base_address_register(uint8_t bus, uint8_t device, uint8_t function,
                                                uint8_t bar) {
    base_address_register result = {0, NULL, 0, 0};

    uint32_t headertype = read_pci(bus, device, function, 0x0e) & 0x7e;
    int      max_bars   = 6 - 4 * headertype;
    if (bar >= max_bars) return result;

    uint32_t bar_value = read_pci(bus, device, function, 0x10 + 4 * bar);
    result.type        = (bar_value & 1) ? input_output : mem_mapping;

    if (result.type == mem_mapping) {
        switch ((bar_value >> 1) & 0x3) {
        case 0: // 32
        case 1: // 20
        case 2: // 64
            break;
        }
        result.address      = (uint8_t *)(bar_value & ~0x3);
        result.prefetchable = 0;
    } else {
        result.address      = (uint8_t *)(bar_value & ~0x3);
        result.prefetchable = 0;
    }
    return result;
}

void pci_config(unsigned int bus, unsigned int f, unsigned int equipment, unsigned int adder) {
    unsigned int cmd = 0;
    cmd              = 0x80000000 + (unsigned int)adder + ((unsigned int)f << 8) +
          ((unsigned int)equipment << 11) + ((unsigned int)bus << 16);
    // cmd = cmd | 0x01;
    io_out32(PCI_COMMAND_PORT, cmd);
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

pci_device_t *pci_find_vid_did(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < device_number; i++) {
        if (pci_device[i]->vendor_id == vendor_id && pci_device[i]->device_id == device_id)
            return pci_device[i];
    }
    return NULL;
}

pci_device_t *pci_find_class(uint32_t class_code) {
    for (int i = 0; i < device_number; i++) {
        if (pci_device[i]->class_code == class_code) { return pci_device[i]; }
        if (class_code == (pci_device[i]->class_code & 0xFFFF00)) { return pci_device[i]; }
    }
    return NULL;
}

base_address_register find_bar(pci_device_t *device, uint8_t barNum) {
    base_address_register bar =
        get_base_address_register(device->bus, device->slot, device->func, barNum);
    return bar;
}

void load_pci_device(uint32_t BUS, uint32_t Equipment, uint32_t F) {
    uint32_t value_c    = read_pci(BUS, Equipment, F, PCI_CONF_REVISION);
    uint32_t class_code = value_c >> 8;

    uint16_t value_v   = read_pci(BUS, Equipment, F, PCI_CONF_VENDOR);
    uint16_t value_d   = read_pci(BUS, Equipment, F, PCI_CONF_DEVICE);
    uint16_t vendor_id = value_v & 0xffff;
    uint16_t device_id = value_d & 0xffff;

    pci_device_t *device = kmalloc(sizeof(pci_device_t));
    device->name         = pci_classname(class_code);
    device->vendor_id    = vendor_id;
    device->device_id    = device_id;
    device->class_code   = class_code;
    device->bus          = BUS;
    device->slot         = Equipment;
    device->func         = F;

    if (device_number > PCI_DEVICE_MAX) {
        printk("add device full %d\n", device_number);
        return;
    }
    pci_device[device_number++] = device;

    logkf("Found PCI device: %03d:%02d:%02d [0x%04X:0x%04X] <%08x> %s\n", device->bus, device->slot,
          device->func, device->vendor_id, device->device_id, device->class_code, device->name);
}

void print_all_pci() {
    printk("Bus:Slot:Func\t[Vendor:Device]\tClass Code\tName\n");
    for (int i = 0; i < device_number; i++) {
        pci_device_t *device = pci_device[i];
        printk("%03d:%02d:%02d\t[0x%04X:0x%04X]\t<0x%08x>\t%s\n", device->bus, device->slot,
               device->func, device->vendor_id, device->device_id, device->class_code,
               device->name);
    }
}

void init_pci() {
    PCI_ADDR_BASE = (uint32_t)kmalloc(1 * 1024 * 1024);
    unsigned int   i, BUS, Equipment, F, ADDER, *i1;
    unsigned char *PCI_DATA = (char *)PCI_ADDR_BASE, *PCI_DATA1;

    for (BUS = 0; BUS < 256; BUS++) {                      //查询总线
        for (Equipment = 0; Equipment < 32; Equipment++) { //查询设备
            for (F = 0; F < 8; F++) {                      //查询功能
                pci_config(BUS, F, Equipment, 0);
                if (io_in32(PCI_DATA_PORT) != 0xFFFFFFFF) {
                    //当前插槽有设备
                    //把当前设备信息映射到PCI数据区
                    int key = 1;
                    while (key) {
                        PCI_DATA1  = PCI_DATA;
                        *PCI_DATA1 = 0xFF; //表占用标志
                        PCI_DATA1++;
                        *PCI_DATA1 = BUS; //总线号
                        PCI_DATA1++;
                        *PCI_DATA1 = Equipment; //设备号
                        PCI_DATA1++;
                        *PCI_DATA1 = F; //功能号
                        PCI_DATA1++;
                        PCI_DATA1 = PCI_DATA1 + 8;
                        for (ADDER = 0; ADDER < 256; ADDER = ADDER + 4) {
                            pci_config(BUS, F, Equipment, ADDER);
                            i  = io_in32(PCI_DATA_PORT);
                            i1 = (uint32_t *)i;
                            //*i1 = PCI_DATA1;
                            memcpy(PCI_DATA1, &i, 4);
                            PCI_DATA1 = PCI_DATA1 + 4;
                        }
                        for (uint8_t barNum = 0; barNum < 6; barNum++) {
                            base_address_register bar =
                                get_base_address_register(BUS, Equipment, F, barNum);
                            if (bar.address && (bar.type == input_output)) {
                                PCI_DATA1 += 4;
                                int i      = ((uint32_t)(bar.address));
                                memcpy(PCI_DATA1, &i, 4);
                            }
                        }
                        PCI_DATA = PCI_DATA + 0x110 + 4;
                        key      = 0;
                    }
                    PCI_NUM++;
                    load_pci_device(BUS, Equipment, F);
                }
            }
        }
    }
    klogf(true, "PCI device loaded: %d\n", PCI_NUM);
}