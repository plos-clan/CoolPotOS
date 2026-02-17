#pragma once

#include "types.h"

#define TRB_CYCLE (1u << 0)
#define TRB_ENT   (1u << 1)
#define TRB_ISP   (1u << 2)
#define TRB_NS    (1u << 3)
#define TRB_CHAIN (1u << 4)
#define TRB_IOC   (1u << 5)
#define TRB_IDT   (1u << 6)

#define TRB_NORMAL             1
#define TRB_SETUP_STAGE        2
#define TRB_DATA_STAGE         3
#define TRB_STATUS_STAGE       4
#define TRB_LINK               6
#define TRB_ENABLE_SLOT        9
#define TRB_DISABLE_SLOT       10
#define TRB_ADDRESS_DEVICE     11
#define TRB_CONFIGURE_ENDPOINT 12
#define TRB_NO_OP_CMD          23
#define TRB_TRANSFER_EVENT     32
#define TRB_CMD_COMPLETION     33
#define TRB_PORT_STATUS_CHANGE 34

typedef struct __attribute__((packed)) Trb {
    uint32_t param_low;
    uint32_t param_high;
    uint32_t status;
    uint32_t control;
} Trb;

uint32_t trb_get_type(Trb trb);
uint8_t trb_slot_id(Trb trb);
uint32_t trb_endpoint_id(Trb trb);
uint32_t trb_completion_code(Trb trb);
uint32_t trb_transfer_length(Trb trb);

Trb trb_new_no_op_cmd(void);
Trb trb_new_normal(uint64_t buffer, uint32_t len);
Trb trb_new_enable_slot(void);
Trb trb_new_disable_slot(uint8_t slot_id);
Trb trb_new_setup_stage(uint32_t req_low, uint32_t req_high, uint32_t trt);
Trb trb_new_data_stage(uint64_t buffer, uint32_t len, bool dir_in);
Trb trb_new_status_stage(bool dir_in);
Trb trb_new_address_device(uint64_t ctx_ptr, uint8_t slot_id);
Trb trb_new_configure_endpoint(uint64_t ctx_ptr, uint8_t slot_id);
