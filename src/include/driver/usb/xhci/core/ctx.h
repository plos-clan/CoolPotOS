#pragma once

#include "types.h"

typedef struct __attribute__((packed)) ErstEntry {
    uint64_t base_addr;
    uint32_t size;
    uint32_t reserved;
} ErstEntry;

typedef struct __attribute__((packed)) SlotContext {
    uint32_t info1;
    uint32_t info2;
    uint32_t tt_id;
    uint32_t state;
    uint32_t reserved[4];
} SlotContext;

SlotContext *slot_context_from(uintptr_t base, int ctx_size);
void         slot_context_set_entries(SlotContext *ctx, uint32_t count);
void         slot_context_set_root_hub_port(SlotContext *ctx, uint32_t port);
void         slot_context_set_speed(SlotContext *ctx, uint32_t speed);
void         slot_context_set_route_string(SlotContext *ctx, uint32_t route);


typedef struct __attribute__((packed)) InputControlContext {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[6];
} InputControlContext;

InputControlContext *input_control_context_from(uintptr_t base);


typedef struct __attribute__((packed)) EndpointContext {
    uint32_t info1;
    uint32_t info2;
    uint32_t tr_dequeue_low;
    uint32_t tr_dequeue_high;
    uint32_t tx_info;
    uint32_t reserved[3];
} EndpointContext;

EndpointContext *endpoint_context_from(uintptr_t base, int dci, int ctx_size);
void endpoint_context_set_mult(EndpointContext *ctx, uint32_t val);
void endpoint_context_set_interval(EndpointContext *ctx, uint32_t val);
void endpoint_context_set_ep_type(EndpointContext *ctx, uint32_t val);
void endpoint_context_set_max_packet_size(EndpointContext *ctx, uint32_t size);
void endpoint_context_set_error_count(EndpointContext *ctx, uint32_t count);
void endpoint_context_set_max_burst(EndpointContext *ctx, uint32_t size);
void endpoint_context_set_average_trb_len(EndpointContext *ctx, uint32_t len);
void endpoint_context_set_max_esit_payload(EndpointContext *ctx, uint32_t size);
void endpoint_context_set_dequeue_ptr(EndpointContext *ctx, uint64_t ptr);
