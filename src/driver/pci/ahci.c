#include "ahci.h"
#include "pci.h"
#include "hhdm.h"
#include "kprint.h"

void ahci_setup(){
    pci_device_t device = pci_find_class(0x010601);
    if (device == NULL) {
        return;
    } else kinfo("Find Serial ATA Controller.");

    HBA_MEM *hba_mem = phys_to_virt((uint64_t)read_bar_n(device,5));

    //允许产生中断
    uint32_t conf = pci_read_command_status(device);
    conf &= 0xffff0000;
    conf |= 0x7;
    pci_write_command_status(device, conf);

    hba_mem->ghc |= (1 << 31);
}
