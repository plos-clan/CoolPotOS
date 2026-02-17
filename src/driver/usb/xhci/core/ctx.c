#include "driver/usb/xhci/core/ctx.h"

SlotContext *slot_context_from(uintptr_t base, int ctx_size) {
    return (SlotContext *)((uint8_t *)base + ctx_size);
}

void slot_context_set_entries(SlotContext *ctx, uint32_t count) {
    ctx->info1 |= (count & 0x1fu) << 27;
}

void slot_context_set_root_hub_port(SlotContext *ctx, uint32_t port) {
    ctx->info2 |= (port & 0xffu) << 16;
}

void slot_context_set_speed(SlotContext *ctx, uint32_t speed) {
    ctx->info1 |= (speed & 0xfu) << 20;
}

void slot_context_set_route_string(SlotContext *ctx, uint32_t route) {
    ctx->info1 |= (route & 0xfffffu);
}

InputControlContext *input_control_context_from(uintptr_t base) {
    return (InputControlContext *)base;
}

EndpointContext *endpoint_context_from(uintptr_t base, int dci, int ctx_size) {
    int offset = (dci + 1) * ctx_size;
    return (EndpointContext *)((uint8_t *)base + offset);
}

void endpoint_context_set_mult(EndpointContext *ctx, uint32_t val) {
    ctx->info1 |= (val & 0x3u) << 8;
}

void endpoint_context_set_interval(EndpointContext *ctx, uint32_t val) {
    ctx->info1 |= (val & 0xffu) << 16;
}

void endpoint_context_set_ep_type(EndpointContext *ctx, uint32_t val) {
    ctx->info2 |= (val & 0x7u) << 3;
}

void endpoint_context_set_max_packet_size(EndpointContext *ctx, uint32_t size) {
    ctx->info2 |= (size & 0xffffu) << 16;
}

void endpoint_context_set_error_count(EndpointContext *ctx, uint32_t count) {
    ctx->info2 |= (count & 0x3u) << 1;
}

void endpoint_context_set_max_burst(EndpointContext *ctx, uint32_t size) {
    ctx->info2 |= (size & 0xffu) << 8;
}

void endpoint_context_set_average_trb_len(EndpointContext *ctx, uint32_t len) {
    ctx->tx_info |= (len & 0xffffu);
}

void endpoint_context_set_max_esit_payload(EndpointContext *ctx, uint32_t size) {
    ctx->tx_info |= (size & 0xffffu) << 16;
}

void endpoint_context_set_dequeue_ptr(EndpointContext *ctx, uint64_t ptr) {
    ctx->tr_dequeue_low = (uint32_t)ptr | 1u;
    ctx->tr_dequeue_high = (uint32_t)(ptr >> 32);
}
