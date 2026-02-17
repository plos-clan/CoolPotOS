#pragma once

#include "types.h"
#include "driver/usb/xhci/core/trb.h"

typedef struct CommandRing {
    Trb *base;
    uint64_t phys_addr;
    uint32_t capacity;
    uint32_t enqueue_idx;
    bool cycle_state;
} CommandRing;

CommandRing command_ring_new(void);
void command_ring_enqueue(CommandRing *ring, Trb trb);

typedef struct EventRing {
    Trb *base;
    uint64_t phys_addr;
    uint32_t capacity;
    uint32_t dequeue_idx;
    bool cycle_state;
    uintptr_t erdp_reg;
} EventRing;

EventRing event_ring_new(uintptr_t erdp_reg);
bool event_ring_has_event(EventRing *ring);
bool event_ring_pop(EventRing *ring, Trb *out_trb);
void event_ring_update_erdp(EventRing *ring);

typedef struct TransferRing {
    Trb *base;
    uint64_t phys_addr;
    uint32_t capacity;
    uint32_t enqueue_idx;
    bool cycle_state;
} TransferRing;

TransferRing transfer_ring_new(void);
void transfer_ring_enqueue(TransferRing *ring, Trb trb);
void transfer_ring_free(TransferRing *ring);
