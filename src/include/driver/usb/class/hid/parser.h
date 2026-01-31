#pragma once

#include "types.h"
#include "driver/usb/usb_vec.h"

#define HID_ITEM_TYPE_MAIN   0
#define HID_ITEM_TYPE_GLOBAL 1
#define HID_ITEM_TYPE_LOCAL  2

#define HID_TAG_INPUT          0b1000
#define HID_TAG_OUTPUT         0b1001
#define HID_TAG_COLLECTION     0b1010
#define HID_TAG_FEATURE        0b1011
#define HID_TAG_END_COLLECTION 0b1100

#define HID_TAG_USAGE_PAGE   0b0000
#define HID_TAG_LOGICAL_MIN  0b0001
#define HID_TAG_LOGICAL_MAX  0b0010
#define HID_TAG_PHYSICAL_MIN 0b0011
#define HID_TAG_PHYSICAL_MAX 0b0100
#define HID_TAG_UNIT_EXP     0b0101
#define HID_TAG_UNIT         0b0110
#define HID_TAG_REPORT_SIZE  0b0111
#define HID_TAG_REPORT_ID    0b1000
#define HID_TAG_REPORT_COUNT 0b1001
#define HID_TAG_PUSH         0b1010
#define HID_TAG_POP          0b1011

#define HID_TAG_USAGE     0b0000
#define HID_TAG_USAGE_MIN 0b0001
#define HID_TAG_USAGE_MAX 0b0010

#define HID_FLAG_CONSTANT    (1u << 0)
#define HID_FLAG_VARIABLE    (1u << 1)
#define HID_FLAG_RELATIVE    (1u << 2)
#define HID_FLAG_WRAP        (1u << 3)
#define HID_FLAG_NONLINEAR   (1u << 4)
#define HID_FLAG_NO_PREF     (1u << 5)
#define HID_FLAG_NULL_STATE  (1u << 6)
#define HID_FLAG_VOLATILE    (1u << 7)
#define HID_FLAG_BUFFERED    (1u << 8)

typedef enum {
    HID_KIND_INPUT   = 0,
    HID_KIND_OUTPUT  = 1,
    HID_KIND_FEATURE = 2,
} HidKind;

typedef struct HidField {
    uint8_t  report_id;
    HidKind  kind;
    uint32_t bit_offset;
    uint32_t bit_size;
    uint32_t report_count;
    int32_t  logical_min;
    int32_t  logical_max;
    int32_t  physical_min;
    int32_t  physical_max;
    uint32_t flags;
    uint16_t usage_page;
    uint32_t usage_min;
    uint32_t usage_max;
} HidField;

USB_VEC_DEFINE(HidField, HidFieldVec);

bool     hid_field_is_const(const HidField *field);
bool     hid_field_is_variable(const HidField *field);
bool     hid_field_is_array(const HidField *field);
bool     hid_field_is_relative(const HidField *field);
bool     hid_field_is_range(const HidField *field);
uint32_t hid_field_value(const HidField *field, const uint8_t *data, uint32_t idx);
int32_t  hid_field_value_signed(const HidField *field, const uint8_t *data, uint32_t idx);

typedef struct HidReport {
    uint8_t     id;
    uint32_t    size_bits[3];
    HidFieldVec fields;
} HidReport;

uint32_t hid_report_size_bytes(const HidReport *report, HidKind kind);

typedef struct HidReportMap {
    bool     used[256];
    HidReport values[256];
    uint16_t len;
} HidReportMap;

void      hid_report_map_init(HidReportMap *map);
void      hid_report_map_free(HidReportMap *map);
HidReport *hid_report_map_get(HidReportMap *map, uint8_t report_id);
HidReport *hid_report_map_ensure(HidReportMap *map, uint8_t report_id, HidReport *templ);

typedef struct HidDescriptor {
    HidReportMap reports;
} HidDescriptor;

void hid_descriptor_free(HidDescriptor *desc);

typedef struct LocalItem {
    bool   is_range;
    uint32_t min;
    uint32_t max;
} LocalItem;

USB_VEC_DEFINE(LocalItem, LocalItemVec);

typedef struct LocalState {
    LocalItemVec items;
} LocalState;

typedef struct GlobalState {
    uint16_t usage_page;
    int32_t  logical_min;
    int32_t  logical_max;
    int32_t  physical_min;
    int32_t  physical_max;
    uint32_t report_size;
    uint32_t report_count;
    uint8_t  report_id;
} GlobalState;

USB_VEC_DEFINE(GlobalState, GlobalStateVec);

typedef struct HidParser {
    const uint8_t *data;
    uint16_t       length;
    uint16_t       offset;
    GlobalState    global;
    GlobalStateVec global_stack;
    LocalState     local;
    HidDescriptor  descriptor;
} HidParser;

void hid_parser_init(HidParser *parser, const uint8_t *data, uint16_t length);
bool hid_parser_parse(HidParser *parser, HidDescriptor *out_desc);
