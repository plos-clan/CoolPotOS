#include "driver/usb/xhci/core/trb.h"

uint32_t trb_get_type(Trb trb) {
    return (trb.control >> 10) & 0x3f;
}

uint8_t trb_slot_id(Trb trb) {
    return (uint8_t)((trb.control >> 24) & 0xff);
}

uint32_t trb_endpoint_id(Trb trb) {
    return (trb.control >> 16) & 0x1f;
}

uint32_t trb_completion_code(Trb trb) {
    return (trb.status >> 24) & 0xff;
}

uint32_t trb_transfer_length(Trb trb) {
    return trb.status & 0xffffff;
}

Trb trb_new_no_op_cmd(void) {
    Trb trb = { .control = (uint32_t)TRB_NO_OP_CMD << 10 };
    return trb;
}

Trb trb_new_normal(uint64_t buffer, uint32_t len) {
    Trb trb = {
        .param_low  = (uint32_t)buffer,
        .param_high = (uint32_t)(buffer >> 32),
        .status     = len,
        .control    = ((uint32_t)TRB_NORMAL << 10) | TRB_IOC | TRB_ISP,
    };
    return trb;
}

Trb trb_new_enable_slot(void) {
    Trb trb = { .control = (uint32_t)TRB_ENABLE_SLOT << 10 };
    return trb;
}

Trb trb_new_disable_slot(uint8_t slot_id) {
    Trb trb = { .control = ((uint32_t)TRB_DISABLE_SLOT << 10) | ((uint32_t)slot_id << 24) };
    return trb;
}

Trb trb_new_setup_stage(uint32_t req_low, uint32_t req_high, uint32_t trt) {
    Trb trb = {
        .param_low  = req_low,
        .param_high = req_high,
        .status     = 8,
        .control    = ((uint32_t)TRB_SETUP_STAGE << 10) | TRB_IDT | (trt << 16),
    };
    return trb;
}

Trb trb_new_data_stage(uint64_t buffer, uint32_t len, bool dir_in) {
    uint32_t dir_bit = dir_in ? (1u << 16) : 0u;
    Trb trb          = {
                 .param_low  = (uint32_t)buffer,
                 .param_high = (uint32_t)(buffer >> 32),
                 .status     = len,
                 .control    = ((uint32_t)TRB_DATA_STAGE << 10) | dir_bit | TRB_CHAIN,
    };
    return trb;
}

Trb trb_new_status_stage(bool dir_in) {
    uint32_t dir_bit = dir_in ? (1u << 16) : 0u;
    Trb trb          = { .control = ((uint32_t)TRB_STATUS_STAGE << 10) | dir_bit | TRB_IOC };
    return trb;
}

Trb trb_new_address_device(uint64_t ctx_ptr, uint8_t slot_id) {
    Trb trb = {
        .param_low  = (uint32_t)ctx_ptr,
        .param_high = (uint32_t)(ctx_ptr >> 32),
        .control    = ((uint32_t)TRB_ADDRESS_DEVICE << 10) | ((uint32_t)slot_id << 24),
    };
    return trb;
}

Trb trb_new_configure_endpoint(uint64_t ctx_ptr, uint8_t slot_id) {
    Trb trb = {
        .param_low  = (uint32_t)ctx_ptr,
        .param_high = (uint32_t)(ctx_ptr >> 32),
        .control    = ((uint32_t)TRB_CONFIGURE_ENDPOINT << 10) | ((uint32_t)slot_id << 24),
    };
    return trb;
}
