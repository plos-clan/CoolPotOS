#include "driver/usb/xhci/core/ring.h"
#include "driver/usb/usb_mem.h"
#include "krlibc.h"

static inline void mmio_out32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

CommandRing command_ring_new(void) {
    uint64_t phys      = 0;
    void    *virt      = usb_alloc_dma_pages(1, &phys);
    uint32_t trb_count = (uint32_t)(0x1000 / sizeof(Trb));

    CommandRing ring = {
        .base        = (Trb *)virt,
        .phys_addr   = phys,
        .capacity    = trb_count,
        .enqueue_idx = 0,
        .cycle_state = true,
    };
    return ring;
}

static void command_ring_link_to_start(CommandRing *ring) {
    uint32_t link_idx = ring->enqueue_idx;

    Trb link_trb = {
        .param_low  = (uint32_t)(ring->phys_addr & 0xffffffffu),
        .param_high = (uint32_t)(ring->phys_addr >> 32),
        .status     = 0,
        .control    = ((uint32_t)TRB_LINK << 10),
    };

    link_trb.control |= (1u << 1);

    if (ring->cycle_state) {
        link_trb.control |= 1u;
    } else {
        link_trb.control &= ~1u;
    }

    ring->base[link_idx] = link_trb;
    ring->enqueue_idx    = 0;
    ring->cycle_state    = !ring->cycle_state;
}

void command_ring_enqueue(CommandRing *ring, Trb trb) {
    if (ring->enqueue_idx == ring->capacity - 1) {
        command_ring_link_to_start(ring);
    }

    uint32_t target_idx = ring->enqueue_idx;
    Trb      write_trb  = trb;

    if (ring->cycle_state) {
        write_trb.control |= 1u;
    } else {
        write_trb.control &= ~1u;
    }

    ring->base[target_idx] = write_trb;
    ring->enqueue_idx++;
}

EventRing event_ring_new(uintptr_t erdp_reg) {
    uint64_t phys      = 0;
    void    *virt      = usb_alloc_dma_pages(1, &phys);
    uint32_t trb_count = (uint32_t)(0x1000 / sizeof(Trb));

    EventRing ring = {
        .base        = (Trb *)virt,
        .phys_addr   = phys,
        .capacity    = trb_count,
        .dequeue_idx = 0,
        .cycle_state = true,
        .erdp_reg    = erdp_reg,
    };
    return ring;
}

bool event_ring_has_event(EventRing *ring) {
    Trb      trb      = ring->base[ring->dequeue_idx];
    uint32_t expected = ring->cycle_state ? 1u : 0u;
    return (trb.control & 1u) == expected;
}

bool event_ring_pop(EventRing *ring, Trb *out_trb) {
    if (!event_ring_has_event(ring)) {
        return false;
    }

    Trb trb = ring->base[ring->dequeue_idx];
    ring->dequeue_idx++;

    if (ring->dequeue_idx == ring->capacity) {
        ring->dequeue_idx = 0;
        ring->cycle_state = !ring->cycle_state;
    }

    if (out_trb) {
        *out_trb = trb;
    }
    return true;
}

void event_ring_update_erdp(EventRing *ring) {
    uint64_t current_phys = ring->phys_addr + (uint64_t)ring->dequeue_idx * 16u;
    uint64_t val_to_write = current_phys | (1u << 3);

    uint32_t low  = (uint32_t)(val_to_write & 0xffffffffu);
    uint32_t high = (uint32_t)(val_to_write >> 32);

    mmio_out32(ring->erdp_reg, low);
    mmio_out32(ring->erdp_reg + 4, high);
}

TransferRing transfer_ring_new(void) {
    uint64_t phys      = 0;
    void    *virt      = usb_alloc_dma_pages(1, &phys);
    uint32_t trb_count = (uint32_t)(0x1000 / sizeof(Trb));

    TransferRing ring = {
        .base        = (Trb *)virt,
        .phys_addr   = phys,
        .capacity    = trb_count,
        .enqueue_idx = 0,
        .cycle_state = true,
    };
    return ring;
}

static void transfer_ring_link_to_start(TransferRing *ring) {
    uint32_t link_idx = ring->enqueue_idx;
    Trb      link_trb = {
             .param_low  = (uint32_t)(ring->phys_addr & 0xffffffffu),
             .param_high = (uint32_t)(ring->phys_addr >> 32),
             .control    = ((uint32_t)TRB_LINK << 10) | (1u << 1),
    };

    if (ring->cycle_state) {
        link_trb.control |= 1u;
    } else {
        link_trb.control &= ~1u;
    }

    ring->base[link_idx] = link_trb;
    ring->enqueue_idx    = 0;
    ring->cycle_state    = !ring->cycle_state;
}

void transfer_ring_enqueue(TransferRing *ring, Trb trb) {
    if (ring->enqueue_idx == ring->capacity - 1) {
        transfer_ring_link_to_start(ring);
    }

    uint32_t target_idx = ring->enqueue_idx;
    Trb      write_trb  = trb;

    if (ring->cycle_state) {
        write_trb.control |= 1u;
    } else {
        write_trb.control &= ~1u;
    }

    ring->base[target_idx] = write_trb;
    ring->enqueue_idx++;
}

void transfer_ring_free(TransferRing *ring) {
    if (ring->phys_addr != 0) {
        usb_free_dma_pages(ring->base, 1);
    }
}
