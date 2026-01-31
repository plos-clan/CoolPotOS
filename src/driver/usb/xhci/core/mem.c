#include "driver/usb/xhci/core/xhci.h"
#include "driver/usb/xhci/regs/int.h"
#include "driver/usb/usb_mem.h"
#include "term/klog.h"

void xhci_setup_command_ring(Xhci *xhci) {
    xhci->cmd_ring = command_ring_new();
    operational_set_crcr(xhci->op, xhci->cmd_ring.phys_addr | 1u);
}

static void xhci_setup_scratchpads(Xhci *xhci, uint32_t count) {
    uint64_t sp_arr_phys = 0;
    void    *sp_arr_virt = usb_alloc_dma_pages(1, &sp_arr_phys);
    uint64_t *sp_arr_ptr = (uint64_t *)sp_arr_virt;

    for (uint32_t i = 0; i < count; i++) {
        uint64_t buf_phys = 0;
        usb_alloc_dma_pages(1, &buf_phys);
        sp_arr_ptr[i] = buf_phys;
    }

    xhci->dcbaa_virt[0] = sp_arr_phys;
}

void xhci_setup_dcbaa(Xhci *xhci, uint8_t max_slots) {
    (void)max_slots;
    uint64_t dcbaa_phys = 0;
    void    *dcbaa_virt = usb_alloc_dma_pages(1, &dcbaa_phys);
    xhci->dcbaa_virt = (uint64_t *)dcbaa_virt;

    uint32_t sp_count = capability_max_scratchpad_bufs(xhci->cap);
    if (sp_count > 0) {
        xhci_setup_scratchpads(xhci, sp_count);
    }

    operational_set_dcbaap(xhci->op, dcbaa_phys);
    kdebug("DCBAA setup at: %#lx", (uint64_t)dcbaa_virt);
}

void xhci_setup_interrupter(Xhci *xhci) {
    uint64_t erst_phys = 0;
    void    *erst_virt = usb_alloc_dma_pages(1, &erst_phys);

    uint32_t rt_off  = capability_rts_off(xhci->cap);
    uintptr_t rt_base = xhci->cap.base_addr + (uintptr_t)rt_off;

    Interrupter ir = interrupter_new(rt_base, 0);
    xhci->event_ring = event_ring_new(interrupter_erdp_addr(ir));

    ErstEntry *entry = (ErstEntry *)erst_virt;
    entry->base_addr = (uint64_t)xhci->event_ring.phys_addr;
    entry->size      = xhci->event_ring.capacity;

    interrupter_set_erstsz(ir, 1);
    interrupter_set_erdp(ir, xhci->event_ring.phys_addr);
    interrupter_set_erstba(ir, erst_phys);
    interrupter_enable(ir);

    kdebug("Event ring segment table: %#lx", (uint64_t)erst_virt);
}
