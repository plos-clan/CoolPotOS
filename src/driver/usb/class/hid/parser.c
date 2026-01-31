#include "driver/usb/class/hid/parser.h"
#include "krlibc.h"
#include "term/klog.h"

bool hid_field_is_const(const HidField *field) {
    return (field->flags & HID_FLAG_CONSTANT) != 0;
}

bool hid_field_is_variable(const HidField *field) {
    return (field->flags & HID_FLAG_VARIABLE) != 0;
}

bool hid_field_is_array(const HidField *field) {
    return (field->flags & HID_FLAG_VARIABLE) == 0;
}

bool hid_field_is_relative(const HidField *field) {
    return (field->flags & HID_FLAG_RELATIVE) != 0;
}

bool hid_field_is_range(const HidField *field) {
    return field->usage_min != field->usage_max;
}

uint32_t hid_field_value(const HidField *field, const uint8_t *data, uint32_t idx) {
    uint32_t offset   = field->bit_offset + (idx * field->bit_size);
    uint32_t byte_idx = offset / 8;
    uint32_t shift    = offset % 8;

    uint32_t count = (offset + field->bit_size + 7) / 8 - byte_idx;
    uint64_t raw   = 0;
    for (uint32_t i = 0; i < count; i++) {
        raw |= (uint64_t)data[byte_idx + i] << (i * 8);
    }
    return (uint32_t)((raw >> shift) & ((1ULL << field->bit_size) - 1));
}

int32_t hid_field_value_signed(const HidField *field, const uint8_t *data, uint32_t idx) {
    uint32_t val = hid_field_value(field, data, idx);

    if (field->bit_size >= 32) {
        return (int32_t)val;
    }

    uint32_t shift = 32 - field->bit_size;
    return (int32_t)(val << shift) >> shift;
}

uint32_t hid_report_size_bytes(const HidReport *report, HidKind kind) {
    uint32_t idx = (uint32_t)kind;
    return (report->size_bits[idx] + 7) / 8;
}

void hid_report_map_init(HidReportMap *map) {
    memset(map, 0, sizeof(HidReportMap));
}

void hid_report_map_free(HidReportMap *map) {
    if (!map) {
        return;
    }
    for (uint32_t i = 0; i < 256; i++) {
        if (map->used[i]) {
            HidFieldVec_free(&map->values[i].fields);
            map->used[i] = false;
        }
    }
    map->len = 0;
}

HidReport *hid_report_map_get(HidReportMap *map, uint8_t report_id) {
    if (!map->used[report_id]) {
        return NULL;
    }
    return &map->values[report_id];
}

HidReport *hid_report_map_ensure(HidReportMap *map, uint8_t report_id, HidReport *templ) {
    if (!map->used[report_id]) {
        map->used[report_id] = true;
        map->values[report_id] = *templ;
        map->len++;
    }
    return &map->values[report_id];
}

void hid_descriptor_free(HidDescriptor *desc) {
    hid_report_map_free(&desc->reports);
}

void hid_parser_init(HidParser *parser, const uint8_t *data, uint16_t length) {
    memset(parser, 0, sizeof(HidParser));
    parser->data   = data;
    parser->length = length;
    parser->offset = 0;
    HidReportMap map;
    hid_report_map_init(&map);
    parser->descriptor.reports = map;
    LocalItemVec_init(&parser->local.items);
    GlobalStateVec_init(&parser->global_stack);
}

static uint32_t hid_parser_read_unsigned(HidParser *parser, uint16_t len) {
    uint32_t value = 0;
    switch (len) {
    case 1:
        value = (uint32_t)parser->data[parser->offset];
        break;
    case 2: {
        uint32_t b0 = (uint32_t)parser->data[parser->offset];
        uint32_t b1 = (uint32_t)parser->data[parser->offset + 1];
        value       = b0 | (b1 << 8);
        break;
    }
    case 4: {
        uint32_t b0 = (uint32_t)parser->data[parser->offset];
        uint32_t b1 = (uint32_t)parser->data[parser->offset + 1];
        uint32_t b2 = (uint32_t)parser->data[parser->offset + 2];
        uint32_t b3 = (uint32_t)parser->data[parser->offset + 3];
        value       = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
        break;
    }
    default:
        break;
    }

    parser->offset = (uint16_t)(parser->offset + len);
    return value;
}

static int32_t hid_parser_read_signed(HidParser *parser, uint16_t len) {
    uint32_t value_u = hid_parser_read_unsigned(parser, len);
    switch (len) {
    case 1:
        return (int32_t)(int8_t)value_u;
    case 2:
        return (int32_t)(int16_t)value_u;
    case 4:
        return (int32_t)value_u;
    default:
        return 0;
    }
}

static void hid_parser_handle_global(HidParser *parser, uint8_t tag, uint16_t len) {
    switch (tag) {
    case HID_TAG_USAGE_PAGE:
        parser->global.usage_page = (uint16_t)hid_parser_read_unsigned(parser, len);
        break;
    case HID_TAG_LOGICAL_MIN:
        parser->global.logical_min = hid_parser_read_signed(parser, len);
        break;
    case HID_TAG_LOGICAL_MAX:
        parser->global.logical_max = hid_parser_read_signed(parser, len);
        break;
    case HID_TAG_PHYSICAL_MIN:
        parser->global.physical_min = hid_parser_read_signed(parser, len);
        break;
    case HID_TAG_PHYSICAL_MAX:
        parser->global.physical_max = hid_parser_read_signed(parser, len);
        break;
    case HID_TAG_REPORT_SIZE:
        parser->global.report_size = hid_parser_read_unsigned(parser, len);
        break;
    case HID_TAG_REPORT_COUNT:
        parser->global.report_count = hid_parser_read_unsigned(parser, len);
        break;
    case HID_TAG_REPORT_ID:
        parser->global.report_id = (uint8_t)hid_parser_read_unsigned(parser, len);
        break;
    case HID_TAG_PUSH:
        GlobalStateVec_push(&parser->global_stack, parser->global);
        break;
    case HID_TAG_POP: {
        if (parser->global_stack.len > 0) {
            parser->global = parser->global_stack.data[parser->global_stack.len - 1];
            parser->global_stack.len--;
        } else {
            kwarn("HID: Global stack pop underflow");
        }
        break;
    }
    default:
        parser->offset = (uint16_t)(parser->offset + len);
        break;
    }
}

static void hid_parser_handle_local(HidParser *parser, uint8_t tag, uint16_t data_len,
                                    uint32_t val) {
    bool is_extended = data_len == 4;

    uint32_t full_usage = is_extended ? val : ((uint32_t)parser->global.usage_page << 16) | val;

    switch (tag) {
    case HID_TAG_USAGE: {
        LocalItem item = {
            .is_range = false,
            .min      = full_usage,
            .max      = full_usage,
        };
        LocalItemVec_push(&parser->local.items, item);
        break;
    }
    case HID_TAG_USAGE_MIN: {
        LocalItem item = {
            .is_range = true,
            .min      = full_usage,
            .max      = 0,
        };
        LocalItemVec_push(&parser->local.items, item);
        break;
    }
    case HID_TAG_USAGE_MAX: {
        LocalItem *last = LocalItemVec_last(&parser->local.items);
        if (last) {
            last->max = full_usage;
        }
        break;
    }
    default:
        break;
    }
}

static void hid_parser_handle_main(HidParser *parser, uint8_t tag, uint32_t flags) {
    HidKind kind;
    switch (tag) {
    case HID_TAG_INPUT:
        kind = HID_KIND_INPUT;
        break;
    case HID_TAG_OUTPUT:
        kind = HID_KIND_OUTPUT;
        break;
    case HID_TAG_FEATURE:
        kind = HID_KIND_FEATURE;
        break;
    default:
        parser->local.items.len = 0;
        return;
    }

    uint32_t kind_idx  = (uint32_t)kind;
    uint8_t  report_id = parser->global.report_id;

    HidReport new_report;
    memset(&new_report, 0, sizeof(HidReport));
    new_report.id = report_id;
    HidFieldVec_init(&new_report.fields);

    if (report_id != 0) {
        new_report.size_bits[0] = 8;
        new_report.size_bits[1] = 8;
        new_report.size_bits[2] = 8;
    }

    HidReport *layout = hid_report_map_ensure(&parser->descriptor.reports, report_id, &new_report);

    uint32_t report_count    = parser->global.report_count;
    uint32_t report_size     = parser->global.report_size;
    bool     is_variable     = (flags & HID_FLAG_VARIABLE) != 0;
    bool     is_single_range =
        (parser->local.items.len == 1 && parser->local.items.data[0].is_range);

    if (!is_variable && is_single_range) {
        LocalItem usage_item = parser->local.items.data[0];
        HidField  field      = {
            .report_id    = report_id,
            .kind         = kind,
            .bit_offset   = layout->size_bits[kind_idx],
            .bit_size     = report_size,
            .report_count = report_count,
            .logical_min  = parser->global.logical_min,
            .logical_max  = parser->global.logical_max,
            .physical_min = parser->global.physical_min,
            .physical_max = parser->global.physical_max,
            .flags        = flags,
            .usage_page   = parser->global.usage_page,
            .usage_min    = usage_item.min,
            .usage_max    = usage_item.max,
        };
        HidFieldVec_push(&layout->fields, field);
        layout->size_bits[kind_idx] += report_size * report_count;
    } else {
        uint64_t item_idx     = 0;
        uint32_t range_offset = 0;
        for (uint32_t i = 0; i < report_count; i++) {
            uint32_t current_usage = 0;
            if (item_idx < parser->local.items.len) {
                LocalItem item = parser->local.items.data[item_idx];
                current_usage  = item.min + range_offset;
                if (current_usage < item.max) {
                    range_offset++;
                } else if (item_idx < parser->local.items.len - 1) {
                    item_idx++;
                    range_offset = 0;
                }
            }

            HidField field = {
                .report_id    = report_id,
                .kind         = kind,
                .bit_offset   = layout->size_bits[kind_idx],
                .bit_size     = report_size,
                .report_count = 1,
                .logical_min  = parser->global.logical_min,
                .logical_max  = parser->global.logical_max,
                .physical_min = parser->global.physical_min,
                .physical_max = parser->global.physical_max,
                .flags        = flags,
                .usage_page   = parser->global.usage_page,
                .usage_min    = current_usage,
                .usage_max    = current_usage,
            };
            HidFieldVec_push(&layout->fields, field);
            layout->size_bits[kind_idx] += report_size;
        }
    }

    parser->local.items.len = 0;
}

bool hid_parser_parse(HidParser *parser, HidDescriptor *out_desc) {
    while (parser->offset < parser->length) {
        uint8_t header = parser->data[parser->offset];
        parser->offset++;

        uint8_t  size_code = header & 0x03;
        uint16_t data_len  = (size_code == 3) ? 4 : size_code;

        uint8_t item_type = (header >> 2) & 0x03;
        uint8_t item_tag  = (header >> 4) & 0x0f;

        if ((uint16_t)(parser->offset + data_len) > parser->length) {
            kwarn("HID: Truncated at offset %d", parser->offset);
            break;
        }

        switch (item_type) {
        case HID_ITEM_TYPE_MAIN: {
            uint32_t flags = hid_parser_read_unsigned(parser, data_len);
            hid_parser_handle_main(parser, item_tag, flags);
            break;
        }
        case HID_ITEM_TYPE_GLOBAL:
            hid_parser_handle_global(parser, item_tag, data_len);
            break;
        case HID_ITEM_TYPE_LOCAL: {
            uint32_t val = hid_parser_read_unsigned(parser, data_len);
            hid_parser_handle_local(parser, item_tag, data_len, val);
            break;
        }
        default:
            break;
        }
    }

    *out_desc = parser->descriptor;
    return true;
}
