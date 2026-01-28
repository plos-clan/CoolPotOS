#include "lib/neoacpi/libaml.h"
#include "lib/neoacpi/neo_impl.h"
#include "lib/neoacpi/neoacpi.h"
#include "lib/neoacpi/neotable.h"

/* 解析 PkgLength */
static uint32_t aml_parse_pkg_length(aml_context_t *ctx, uint32_t *bytes_read) {
    if (ctx->curr_ptr >= ctx->end_ptr) return 0;

    uint8_t  lead       = *ctx->curr_ptr++;
    uint8_t  byte_count = (lead >> 6) & 0x03;
    uint32_t length     = 0;

    if (byte_count == 0) {
        length      = lead & 0x3F;
        *bytes_read = 1;
    } else {
        if (ctx->curr_ptr + byte_count > ctx->end_ptr) return 0;
        length = lead & 0x0F;
        for (uint8_t i = 0; i < byte_count; i++) {
            length |= ((uint32_t)*ctx->curr_ptr++) << (4 + i * 8);
        }
        *bytes_read = byte_count + 1;
    }
    return length;
}

/* 解析单一 4 字节 NameSeg */
static void aml_parse_name_seg(aml_context_t *ctx, char *out) {
    for (int i = 0; i < 4; i++) {
        if (ctx->curr_ptr < ctx->end_ptr) {
            out[i] = (char)*ctx->curr_ptr++;
        } else {
            out[i] = ' ';
        }
    }
    out[4] = '\0';
}

/* 跳过多级 NamePath，确保解析器指针不走丢 */
static void aml_skip_name_path(aml_context_t *ctx) {
    if (ctx->curr_ptr >= ctx->end_ptr) return;

    uint8_t lead = *ctx->curr_ptr;
    if (lead == '\\') ctx->curr_ptr++;
    while (*ctx->curr_ptr == '^')
        ctx->curr_ptr++;

    lead = *ctx->curr_ptr;
    if (lead == 0x2E) {
        ctx->curr_ptr += 1 + 8;
    } else if (lead == 0x2F) {
        uint8_t count  = *(ctx->curr_ptr + 1);
        ctx->curr_ptr += 2 + (count * 4);
    } else if (lead == 0x00) {
        ctx->curr_ptr++;
    } else { // Single NameSeg
        ctx->curr_ptr += 4;
    }
}

/* 跳过数据对象 */
static void aml_skip_data_object(aml_context_t *ctx) {
    if (ctx->curr_ptr >= ctx->end_ptr) return;
    uint8_t op = *ctx->curr_ptr++;

    switch (op) {
    case 0x0A: ctx->curr_ptr += 1; break;
    case 0x0B: ctx->curr_ptr += 2; break;
    case 0x0C: ctx->curr_ptr += 4; break;
    case 0x0D:
        while (*ctx->curr_ptr++ != 0)
            ;
        break;
    case 0x0E: ctx->curr_ptr += 8; break;
    case 0x11:
    case 0x12: {
        uint32_t r;
        uint32_t len   = aml_parse_pkg_length(ctx, &r);
        ctx->curr_ptr += (len - r);
        break;
    }
    case 0x00:
    case 0x01:
    case 0xFF: break;
    }
}

static aml_node_t *aml_create_node(aml_node_t *parent, const char *name, aml_obj_type_t type) {
    aml_node_t *node = (aml_node_t *)neo_acpi_malloc(sizeof(aml_node_t));
    if (!node) return NULL;

    for (int i = 0; i < 4; i++)
        node->name[i] = name[i] ? name[i] : ' ';
    node->name[4]  = '\0';
    node->type     = type;
    node->parent   = parent;
    node->children = node->next = NULL;

    if (parent) {
        if (!parent->children)
            parent->children = node;
        else {
            aml_node_t *tmp = parent->children;
            while (tmp->next)
                tmp = tmp->next;
            tmp->next = node;
        }
    }
    return node;
}

void aml_parse_internal(aml_context_t *ctx, const uint8_t *limit) {
    while (ctx->curr_ptr < limit && ctx->curr_ptr < ctx->end_ptr) {
        uint8_t op = *ctx->curr_ptr++;

        /* 示例，不完善 */
        switch (op) {
        case 0x10: {
            uint32_t       r;
            uint32_t       len       = aml_parse_pkg_length(ctx, &r);
            const uint8_t *scope_end = ctx->curr_ptr - r + len;

            char name[5];
            aml_parse_name_seg(ctx, name);

            aml_node_t *new_scope = aml_create_node(ctx->curr_scope, name, AML_OBJ_TYPE_SCOPE);
            aml_node_t *old_scope = ctx->curr_scope;
            ctx->curr_scope       = new_scope;

            aml_parse_internal(ctx, scope_end);

            ctx->curr_scope = old_scope;
            ctx->curr_ptr   = scope_end;
        } break;
        case 0x5B: {
            uint8_t ext_op = *ctx->curr_ptr++;
            if (ext_op == 0x82 || ext_op == 0x83 || ext_op == 0x85) {
                uint32_t       r;
                uint32_t       len      = aml_parse_pkg_length(ctx, &r);
                const uint8_t *body_end = ctx->curr_ptr - r + len;

                char name[5];
                aml_parse_name_seg(ctx, name);
                aml_create_node(ctx->curr_scope, name, (aml_obj_type_t)ext_op);

                aml_parse_internal(ctx, body_end);
                ctx->curr_ptr = body_end;
            }
        } break;
        case 0x08: {
            char name[5];
            aml_parse_name_seg(ctx, name);
            aml_create_node(ctx->curr_scope, name, AML_OBJ_TYPE_INTEGER);
            aml_skip_data_object(ctx);
        } break;
        case 0x14: {
            uint32_t r;
            uint32_t len   = aml_parse_pkg_length(ctx, &r);
            ctx->curr_ptr += (len - r);

        } break;
        }
        /* 其他 OpCode 处理 */
    }
}

static aml_node_t g_acpi_root_node = {
    .name = "\\", .type = AML_OBJ_TYPE_SCOPE, .parent = NULL, .children = NULL, .next = NULL};

void aml_context_initialize(neo_acpi_handle_t *handle, struct acpi_dsdt *dsdt) {
    handle->aml_context->curr_ptr   = dsdt->definition_block;
    handle->aml_context->end_ptr    = (uint8_t *)dsdt + dsdt->hdr.length;
    handle->aml_context->curr_scope = &g_acpi_root_node;
}
