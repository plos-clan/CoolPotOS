#include "driver/pci/pci.h"
#include "driver/uacpi/acpi.h"
#include "driver/uacpi/tables.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"
#include "term/klog.h"

struct {
    uint32_t    classcode;
    const char *name;
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
    {0x010802, "NVM Express Controller"                     },
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
    {0x000000, (char *)NULL                                 }
};

struct acpi_mcfg_allocation *mcfg_entries[PCI_MCFG_MAX_ENTRIES_LEN];
uint64_t                     mcfg_entries_len = 0;
pci_device_t                *pci_devices[PCI_DEVICE_MAX];
uint32_t                     pci_device_number = 0;

void mcfg_addr_to_entries(struct acpi_mcfg *mcfg, struct acpi_mcfg_allocation **entries,
                          uint64_t *num) {
    struct acpi_mcfg_allocation *entry =
        (struct acpi_mcfg_allocation *)((uint64_t)mcfg + sizeof(struct acpi_mcfg));
    int length = mcfg->hdr.length - sizeof(struct acpi_mcfg);
    *num       = length / sizeof(struct acpi_mcfg_allocation);
    for (uint64_t i = 0; i < *num; i++) {
        entries[i] = entry + i;
    }
}

uint64_t get_device_mmio_physical_address(uint16_t segment_group, uint8_t bus, uint8_t device,
                                          uint8_t function) {
    for (uint64_t i = 0; i < mcfg_entries_len; i++) {
        if (mcfg_entries[i]->segment == segment_group) {
            return mcfg_entries[i]->address +
                   (((uint64_t)bus - (uint64_t)mcfg_entries[i]->start_bus) << 20) +
                   ((uint64_t)device << 15) + ((uint64_t)function << 12);
        }
    }
    return 0;
}

uint64_t get_mmio_address(uint32_t pci_address, uint16_t offset) {
    uint16_t segment  = (pci_address >> 16) & 0xFFFF;
    uint8_t  bus      = (pci_address >> 8) & 0xFF;
    uint8_t  device   = (pci_address >> 3) & 0x1F;
    uint8_t  function = pci_address & 0x07;

    uint64_t phys = get_device_mmio_physical_address(segment, bus, device, function);
    if (phys == 0) { return 0; }
    uint64_t virt = (uint64_t)phys_to_virt(phys);
    page_map_range(get_kernel_pagedir(), virt, phys, PAGE_SIZE * 4, KERNEL_PTE_FLAGS);

    return virt + offset;
}

uint32_t segment_bus_device_functon_to_pci_address(uint16_t segment, uint8_t bus, uint8_t device,
                                                   uint8_t function) {
    return ((uint32_t)(segment & 0xFFFF) << 16) | ((uint32_t)(bus & 0xFF) << 8) |
           ((uint32_t)(device & 0x3F) << 3) | (uint32_t)(function & 0xF);
}

uint32_t pci_read(uint32_t b, uint32_t d, uint32_t f, uint32_t s, uint32_t offset) {
    uint32_t pci_address  = segment_bus_device_functon_to_pci_address(s, b, d, f);
    uint64_t mmio_address = get_mmio_address(pci_address, offset);
    if (mmio_address == 0) { printk("Cannot read pci: failed to get mmio address\n"); }
    return *(volatile uint32_t *)mmio_address;
}

void pci_write(uint32_t b, uint32_t d, uint32_t f, uint32_t s, uint32_t offset, uint32_t value) {
    uint32_t pci_address  = segment_bus_device_functon_to_pci_address(s, b, d, f);
    uint64_t mmio_address = get_mmio_address(pci_address, offset);
    if (mmio_address == 0) { printk("Cannot write pci: failed to get mmio address\n"); }
    *(volatile uint32_t *)mmio_address = value;
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

const char *pci_classname(uint32_t classcode) {
    for (size_t i = 0; pci_classnames[i].name != NULL; i++) {
        if (pci_classnames[i].classcode == classcode) { return pci_classnames[i].name; }
        if (pci_classnames[i].classcode == (classcode & 0xFFFF00)) {
            return pci_classnames[i].name;
        }
    }
    return "Unknown device";
}

void pci_find_vid(pci_device_t **result, uint32_t *n, uint32_t vid) {
    int idx = 0;
    for (uint32_t i = 0; i < pci_device_number; i++) {
        if (pci_devices[i]->vendor_id == vid) {
            result[idx] = pci_devices[i];
            idx++;
            continue;
        }
    }
    *n = idx;
}

void pci_find_class(pci_device_t **result, uint32_t *n, uint32_t class_code) {
    int idx = 0;
    for (uint32_t i = 0; i < pci_device_number; i++) {
        if (pci_devices[i]->class_code == class_code) {
            result[idx] = pci_devices[i];
            idx++;
            continue;
        }
        if (class_code == (pci_devices[i]->class_code & 0xFFFF00)) {
            result[idx] = pci_devices[i];
            idx++;
            continue;
        }
    }
    *n = idx;
}

pci_device_t *pci_find_bdfs(uint8_t bus, uint8_t slot, uint8_t func, uint16_t segment) {
    for (uint32_t i = 0; i < pci_device_number; i++) {
        if ((pci_devices[i]->bus == bus) && (pci_devices[i]->slot == slot) &&
            (pci_devices[i]->func == func) && (pci_devices[i]->segment == segment)) {
            return pci_devices[i];
        }
    }
    return NULL;
}

void pci_scan_function(uint16_t segment_group, uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t pci_address =
        segment_bus_device_functon_to_pci_address(segment_group, bus, device, function);

    uint64_t id_mmio_addr = get_mmio_address(pci_address, 0x00);
    uint16_t vendor_id    = *(volatile uint16_t *)id_mmio_addr;
    if (vendor_id == 0xFFFF) { return; }
    uint16_t device_id = *(volatile uint16_t *)(id_mmio_addr + 2);

    uint64_t field_mmio_addr  = get_mmio_address(pci_address, PCI_CONF_REVISION);
    uint8_t  device_revision  = EXPORT_BYTE(*(volatile uint8_t *)field_mmio_addr, true);
    uint8_t  device_class     = *((uint8_t *)field_mmio_addr + 3);
    uint8_t  device_subclass  = *((uint8_t *)field_mmio_addr + 2);
    uint8_t  device_interface = *((uint8_t *)field_mmio_addr + 1);

    uint64_t header_type_mmio_addr = get_mmio_address(pci_address, 0x0c);
    uint8_t  header_type           = (*((uint8_t *)header_type_mmio_addr + 2)) & 0x7F;

    pci_device_t *pci_device = (pci_device_t *)malloc(sizeof(pci_device_t));
    memset(pci_device, 0, sizeof(pci_device_t));
    pci_device->header_type = header_type;
    pci_device->op          = &pcie_device_op;

    pci_device->revision_id = device_revision;

    pci_device->segment = segment_group;
    pci_device->bus     = bus;
    pci_device->slot    = device;
    pci_device->func    = function;

    uint32_t class_code_24bit = (device_class << 16) | (device_subclass << 8) | device_interface;
    pci_device->class_code    = class_code_24bit;
    pci_device->name          = pci_classname(class_code_24bit);

    switch (header_type) {
    // Endpoint
    case 0x00: {
        uint32_t value  = pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                               pci_device->segment, 0x04);
        value          |= (1 << 2);
        value          |= (1 << 1);
        value          |= (1 << 0);
        pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func,
                              pci_device->segment, 0x04, value);
        pci_device->vendor_id = vendor_id;
        pci_device->device_id = device_id;

        uint32_t subsystem_vendor_device_id = pci_device->op->read(
            pci_device->bus, pci_device->slot, pci_device->func, pci_device->segment, 0x2c);
        uint16_t subsystem_vendor_id = subsystem_vendor_device_id & 0xFFFF;
        uint16_t subsystem_device_id = (subsystem_vendor_device_id >> 16);

        pci_device->subsystem_device_id = subsystem_device_id;
        pci_device->subsystem_vendor_id = subsystem_vendor_id;

        uint32_t interrupt_value = pci_device->op->read(
            pci_device->bus, pci_device->slot, pci_device->func, pci_device->segment, 0x3c);
        pci_device->irq_line = interrupt_value & 0xff;
        pci_device->irq_pin  = (interrupt_value >> 8) & 0xff;

        kinfo("Found PCIe device: %#08lx name: %s", pci_device->class_code, pci_device->name);

        uint32_t capability_point =
            pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                 pci_device->segment, 0x34) &
            0xff;
        pci_device->capability_point = capability_point;

        for (int i = 0; i < 6; i++) {
            int      offset = 0x10 + i * 4;
            uint32_t bar = pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                                pci_device->segment, offset);

            if (bar & 0x01) {
                pci_device->bars[i].address = bar & 0xFFFFFFFC;
                pci_device->bars[i].size    = 0;
                pci_device->bars[i].mmio    = false;
            } else {
                uint64_t bar_address = bar & 0xFFFFFFF0;

                switch ((bar >> 1) & 3) {
                // 32 bit
                case 0b00: {
                    pci_device->bars[i].address = bar & 0xFFFFFFFC;
                    pci_device->bars[i].mmio    = true;

                    uint32_t original_value =
                        pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, offset);

                    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func,
                                          pci_device->segment, offset, 0xFFFFFFFF);
                    uint32_t value =
                        pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, offset);

                    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func,
                                          pci_device->segment, offset, original_value);

                    uint32_t mask = (uint32_t)(value & 0xFFFFFFF0);

                    pci_device->bars[i].size = (uint64_t)(~mask + 1);
                } break;

                // 64 bit
                case 0b10:;
                    uint32_t bar_address_upper =
                        pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, offset + 0x4);

                    bar_address |= ((uint64_t)bar_address_upper << 32);

                    uint32_t original_value =
                        pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, offset);
                    uint32_t original_value_high =
                        pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, offset + 4);

                    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func,
                                          pci_device->segment, offset, 0xFFFFFFFF);
                    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func,
                                          pci_device->segment, offset + 4, 0xFFFFFFFF);
                    uint32_t mask =
                        pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, offset);
                    uint32_t mask_high =
                        pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, offset + 4);

                    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func,
                                          pci_device->segment, offset, original_value);
                    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func,
                                          pci_device->segment, offset + 4, original_value_high);

                    uint64_t value = ((uint64_t)mask_high << 32) | (mask & 0xFFFFFFF0);

                    pci_device->bars[i].size = ~value + 1;

                    pci_device->bars[i].address = bar_address;
                    pci_device->bars[i].mmio    = true;

                    i++;

                    pci_device->bars[i].size = 0;

                    pci_device->bars[i].address = 0;
                    pci_device->bars[i].mmio    = true;
                    break;
                default: break;
                }
            }
        }

        pci_devices[pci_device_number] = pci_device;
        pci_device_number++;

        break;
    }
    // PciPciBridge
    case 0x01: {
        uint32_t data = pci_device->op->read(pci_device->bus, pci_device->slot, pci_device->func,
                                             pci_device->segment, 0x18);
        uint8_t  start_bus = (uint8_t)((data >> 8) & 0xFF);
        uint8_t  end_bus   = (uint8_t)((data >> 16) & 0xFF);
        for (uint8_t bus = start_bus; bus <= end_bus; bus++) {
            pci_scan_bus(segment_group, bus);
        }

        free(pci_device);

        break;
    }
        // CardBusBridge
    case 0x02:
        // Ignore
        break;
    default: printk("Failed to parse header type, header type = %#04x\n", header_type);
    }
}

void pci_scan_bus(uint16_t segment_group, uint8_t bus) {
    for (int i = 0; i < 32; i++) {
        pci_scan_function(segment_group, bus, i, 0);
        uint32_t pci_address = segment_bus_device_functon_to_pci_address(segment_group, bus, i, 0);
        uint64_t mmio_addr   = get_mmio_address(pci_address, 0x0c);
        if (*(volatile uint32_t *)mmio_addr & (1UL << 23)) {
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
    if (*(volatile uint32_t *)mmio_addr & (1UL << 23)) {
        for (int i = 1; i < 8; i++) {
            pci_scan_bus(segment_group, i);
        }
    }
}

void pci_init() {
    struct uacpi_table mcfg_table;
    uacpi_status       status = uacpi_table_find_by_signature("MCFG", &mcfg_table);
    if (status == UACPI_STATUS_OK) {
        // Scan PCIe bus
        mcfg_addr_to_entries((struct acpi_mcfg *)mcfg_table.ptr, mcfg_entries, &mcfg_entries_len);

        for (uint64_t i = 0; i < mcfg_entries_len; i++) {
            uint16_t segment_group = mcfg_entries[i]->segment;
            pci_scan_segment(segment_group);
        }
    } else arch_pci_legacy_enum();
}
