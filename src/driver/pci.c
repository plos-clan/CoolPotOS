#include "../include/pci.h"
#include "../include/memory.h"
#include "../include/io.h"
#include "../include/printf.h"
#include "../include/ide.h"
#include "../include/rtc.h"

unsigned int PCI_ADDR_BASE;
unsigned int PCI_NUM = 0;

struct {
    uint32_t classcode;
    char *name;
} pci_classnames[] = {
        {0x000000, "Non-VGA unclassified device"},
        {0x000100, "VGA compatible unclassified device"},
        {0x010000, "SCSI storage controller"},
        {0x010100, "IDE interface"},
        {0x010200, "Floppy disk controller"},
        {0x010300, "IPI bus controller"},
        {0x010400, "RAID bus controller"},
        {0x018000, "Unknown mass storage controller"},
        {0x020000, "Ethernet controller"},
        {0x020100, "Token ring network controller"},
        {0x020200, "FDDI network controller"},
        {0x020300, "ATM network controller"},
        {0x020400, "ISDN controller"},
        {0x028000, "Network controller"},
        {0x030000, "VGA controller"},
        {0x030100, "XGA controller"},
        {0x030200, "3D controller"},
        {0x038000, "Display controller"},
        {0x040000, "Multimedia video controller"},
        {0x040100, "Multimedia audio controller"},
        {0x040200, "Computer telephony device"},
        {0x048000, "Multimedia controller"},
        {0x050000, "RAM memory"},
        {0x050100, "FLASH memory"},
        {0x058000, "Memory controller"},
        {0x060000, "Host bridge"},
        {0x060100, "ISA bridge"},
        {0x060200, "EISA bridge"},
        {0x060300, "MicroChannel bridge"},
        {0x060400, "PCI bridge"},
        {0x060500, "PCMCIA bridge"},
        {0x060600, "NuBus bridge"},
        {0x060700, "CardBus bridge"},
        {0x060800, "RACEway bridge"},
        {0x060900, "Semi-transparent PCI-to-PCI bridge"},
        {0x060A00, "InfiniBand to PCI host bridge"},
        {0x068000, "Bridge"},
        {0x070000, "Serial controller"},
        {0x070100, "Parallel controller"},
        {0x070200, "Multiport serial controller"},
        {0x070300, "Modem"},
        {0x078000, "Communication controller"},
        {0x080000, "PIC"},
        {0x080100, "DMA controller"},
        {0x080200, "Timer"},
        {0x080300, "RTC"},
        {0x080400, "PCI Hot-plug controller"},
        {0x088000, "System peripheral"},
        {0x090000, "Keyboard controller"},
        {0x090100, "Digitizer Pen"},
        {0x090200, "Mouse controller"},
        {0x090300, "Scanner controller"},
        {0x090400, "Gameport controller"},
        {0x098000, "Input device controller"},
        {0x0A0000, "Generic Docking Station"},
        {0x0A8000, "Docking Station"},
        {0x0B0000, "386"},
        {0x0B0100, "486"},
        {0x0B0200, "Pentium"},
        {0x0B1000, "Alpha"},
        {0x0B2000, "Power PC"},
        {0x0B3000, "MIPS"},
        {0x0B4000, "Co-processor"},
        {0x0C0000, "FireWire (IEEE 1394)"},
        {0x0C0100, "ACCESS Bus"},
        {0x0C0200, "SSA"},
        {0x0C0300, "USB Controller"},
        {0x0C0400, "Fiber Channel"},
        {0x0C0500, "SMBus"},
        {0x0C0600, "InfiniBand"},
        {0x0D0000, "IRDA controller"},
        {0x0D0100, "Consumer IR controller"},
        {0x0D1000, "RF controller"},
        {0x0D8000, "Wireless controller"},
        {0x0E0000, "I2O"},
        {0x0F0000, "Satellite TV controller"},
        {0x0F0100, "Satellite audio communication controller"},
        {0x0F0300, "Satellite voice communication controller"},
        {0x0F0400, "Satellite data communication controller"},
        {0x100000, "Network and computing encryption device"},
        {0x101000, "Entertainment encryption device"},
        {0x108000, "Encryption controller"},
        {0x110000, "DPIO module"},
        {0x110100, "Performance counters"},
        {0x111000, "Communication synchronizer"},
        {0x118000, "Signal processing controller"},
        {0x000000, NULL}
};

uint8_t pci_get_drive_irq(uint8_t bus, uint8_t slot, uint8_t func) {
    return (uint8_t)
    read_pci(bus, slot, func, 0x3c);
}

uint32_t pci_get_port_base(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t io_port = 0;
    for (int i = 0; i < 6; i++) {
        base_address_register bar = get_base_address_register(bus, slot, func, i);
        if (bar.type == input_output) {
            io_port = (uint32_t)
            bar.address;
        }
    }
    return io_port;
}

void PCI_GET_DEVICE(uint16_t vendor_id, uint16_t device_id, uint8_t *bus, uint8_t *slot, uint8_t *func) {
    unsigned char *pci_drive = PCI_ADDR_BASE;
    for (;; pci_drive += 0x110 + 4) {
        if (pci_drive[0] == 0xff) {
            struct pci_config_space_public *pci_config_space_puclic;
            pci_config_space_puclic =
                    (struct pci_config_space_public *) (pci_drive + 0x0c);
            if (pci_config_space_puclic->VendorID == vendor_id &&
                pci_config_space_puclic->DeviceID == device_id) {
                *bus = pci_drive[1];
                *slot = pci_drive[2];
                *func = pci_drive[3];
                return;
            }
        } else {
            break;
        }
    }
}

uint32_t read_bar_n(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_n) {
    uint32_t bar_offset = 0x10 + 4 * bar_n;
    return read_pci(bus, device, function, bar_offset);
}

void write_pci(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset, uint32_t value) {
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

uint32_t read_pci(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset) {
    uint32_t id = 1 << 31 | ((bus & 0xff) << 16) | ((device & 0x1f) << 11) |
                  ((function & 0x07) << 8) | (registeroffset & 0xfc);
    io_out32(PCI_COMMAND_PORT, id);
    uint32_t result = io_in32(PCI_DATA_PORT);
    return result >> (8 * (registeroffset % 4));
}

base_address_register get_base_address_register(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar) {
    base_address_register result;

    uint32_t headertype = read_pci(bus, device, function, 0x0e) & 0x7e;
    int max_bars = 6 - 4 * headertype;
    if (bar >= max_bars)
        return result;

    uint32_t bar_value = read_pci(bus, device, function, 0x10 + 4 * bar);
    result.type = (bar_value & 1) ? input_output : mem_mapping;

    if (result.type == mem_mapping) {
        switch ((bar_value >> 1) & 0x3) {
            case 0:  // 32
            case 1:  // 20
            case 2:  // 64
                break;
        }
        result.address = (uint8_t * )(bar_value & ~0x3);
        result.prefetchable = 0;
    } else {
        result.address = (uint8_t * )(bar_value & ~0x3);
        result.prefetchable = 0;
    }
    return result;
}

void pci_config(unsigned int bus, unsigned int f, unsigned int equipment, unsigned int adder) {
    unsigned int cmd = 0;
    cmd = 0x80000000 + (unsigned int) adder + ((unsigned int) f << 8) +
          ((unsigned int) equipment << 11) + ((unsigned int) bus << 16);
    // cmd = cmd | 0x01;
    io_out32(PCI_COMMAND_PORT, cmd);
}

char *pci_classname(uint32_t classcode) {
    for (size_t i = 0; pci_classnames[i].name != NULL; i++) {
        if (pci_classnames[i].classcode == classcode)
            return pci_classnames[i].name;
        if (pci_classnames[i].classcode == (classcode & 0xFFFF00))
            return pci_classnames[i].name;
    }
    return "Unknown device";
}

void load_pci_device(uint32_t BUS,uint32_t Equipment,uint32_t F){
    uint32_t value_c = read_pci(BUS, Equipment, F, PCI_CONF_REVISION);
    uint32_t class_code = value_c >> 8;

    uint16_t value_v = read_pci(BUS, Equipment, F, PCI_CONF_VENDOR);
    uint16_t value_d = read_pci(BUS, Equipment, F, PCI_CONF_DEVICE);
    uint16_t vendor_id = value_v & 0xffff;
    uint16_t device_id = value_d & 0xffff;

    switch (class_code) {
        case 0x060400:
            return;
    }

    logkf("Found PCI device: %03d:%02d:%02d [0x%04X:0x%04X] %s\n", BUS, Equipment, F, vendor_id, device_id, pci_classname(class_code));
}

void print_all_pci_info(){
    unsigned int BUS, Equipment, F;
    printk("|Bus  Slot  Func  VendorID  DeviceID  ClassCode  Name\n");

    for (BUS = 0; BUS < 256; BUS++) {
        for (Equipment = 0; Equipment < 32; Equipment++) {
            for (F = 0; F < 8; F++) {
                pci_config(BUS, F, Equipment, 0);
                if (io_in32(PCI_DATA_PORT) != 0xFFFFFFFF) {
                    /* 读取class_code */
                    uint32_t value_c = read_pci(BUS, Equipment, F, PCI_CONF_REVISION);
                    uint32_t class_code = value_c >> 8;

                    uint16_t value_v = read_pci(BUS, Equipment, F, PCI_CONF_VENDOR);
                    uint16_t value_d = read_pci(BUS, Equipment, F, PCI_CONF_DEVICE);
                    uint16_t vendor_id = value_v & 0xffff;
                    uint16_t device_id = value_d & 0xffff;

                    /* 打印PCI设备信息 */
                    printk("|%03d  %02d    %02d    0x%04X    0x%04X    0x%06X   %s\n", BUS, Equipment, F, vendor_id, device_id, class_code, pci_classname(class_code));
                }
            }
        }
    }
}

void init_pci() {
    PCI_ADDR_BASE = kmalloc(1 * 1024 * 1024);
    unsigned int i, BUS, Equipment, F, ADDER, *i1;
    unsigned char *PCI_DATA = PCI_ADDR_BASE, *PCI_DATA1;

    for (BUS = 0; BUS < 256; BUS++) {                     //查询总线
        for (Equipment = 0; Equipment < 32; Equipment++) {  //查询设备
            for (F = 0; F < 8; F++) {                         //查询功能
                pci_config(BUS, F, Equipment, 0);
                if (io_in32(PCI_DATA_PORT) != 0xFFFFFFFF) {
                    //当前插槽有设备
                    //把当前设备信息映射到PCI数据区
                    int key = 1;
                    while (key) {
                        PCI_DATA1 = PCI_DATA;
                        *PCI_DATA1 = 0xFF;  //表占用标志
                        PCI_DATA1++;
                        *PCI_DATA1 = BUS;  //总线号
                        PCI_DATA1++;
                        *PCI_DATA1 = Equipment;  //设备号
                        PCI_DATA1++;
                        *PCI_DATA1 = F;  //功能号
                        PCI_DATA1++;
                        PCI_DATA1 = PCI_DATA1 + 8;
                        for (ADDER = 0; ADDER < 256; ADDER = ADDER + 4) {
                            pci_config(BUS, F, Equipment, ADDER);
                            i = io_in32(PCI_DATA_PORT);
                            i1 = i;
                            //*i1 = PCI_DATA1;
                            memcpy(PCI_DATA1, &i, 4);
                            PCI_DATA1 = PCI_DATA1 + 4;
                        }
                        for (uint8_t barNum = 0; barNum < 6; barNum++) {
                            base_address_register bar =
                                    get_base_address_register(BUS, Equipment, F, barNum);
                            if (bar.address && (bar.type == input_output)) {
                                PCI_DATA1 += 4;
                                int i = ((uint32_t)(bar.address));
                                memcpy(PCI_DATA1, &i, 4);
                                PCI_NUM++;
                            }
                        }
                        PCI_DATA = PCI_DATA + 0x110 + 4;
                        key = 0;
                    }
                    load_pci_device(BUS,Equipment,F);
                }
            }
        }
    }
    klogf(true, "PCI device loaded: %d\n", PCI_NUM);
    ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
    rtc_init();
}
