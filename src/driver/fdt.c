#include "driver/fdt.h"
#include "krlibc.h"
#include "boot.h"

/**
 * 初始化FDT解析器
 * @param dtb_phys_addr DTB的物理地址
 * @return 0成功，负值失败
 */
int fdt_init() {
    struct fdt_header *header;
    uint32_t totalsize;

    header = (struct fdt_header *)boot_get_dtb();
    if (!header) {
        return -1;
    }

    /* 检查魔数 */
    if (fdt32_to_cpu(header->magic) != FDT_MAGIC) {
        return -2;
    }

    /* 获取DTB总大小 */
    totalsize = fdt32_to_cpu(header->totalsize);
    g_fdt_ctx.dtb_base = (void *)header;
    if (!g_fdt_ctx.dtb_base) {
        return -3;
    }

    /* 设置各个部分的指针 */
    g_fdt_ctx.header = (struct fdt_header *)g_fdt_ctx.dtb_base;
    g_fdt_ctx.dt_struct =
        (uint8_t *)g_fdt_ctx.dtb_base + fdt32_to_cpu(header->off_dt_struct);
    g_fdt_ctx.dt_strings =
        (char *)g_fdt_ctx.dtb_base + fdt32_to_cpu(header->off_dt_strings);
    g_fdt_ctx.rsv_map =
        (struct fdt_reserve_entry *)((uint8_t *)g_fdt_ctx.dtb_base +
                                     fdt32_to_cpu(header->off_mem_rsvmap));

    return 0;
}

/**
 * 获取属性名称
 */
static const char *fdt_get_property_name(uint32_t nameoff) {
    return g_fdt_ctx.dt_strings + fdt32_to_cpu(nameoff);
}

/**
 * 查找节点
 * @param path 节点路径
 * @return 节点在结构块中的偏移，失败返回-1
 */
int fdt_find_node(const char *path) {
    uint32_t *p = (uint32_t *)g_fdt_ctx.dt_struct;
    int depth = 0;
    char current_path[256] = "";
    char node_name[128];

    if (path[0] != '/') {
        return -1;
    }

    while (1) {
        uint32_t tag = fdt32_to_cpu(*p++);

        switch (tag) {
        case FDT_BEGIN_NODE: {
            const char *name = (const char *)p;
            int name_len = strlen(name);

            if (depth == 0) {
                strcpy(current_path, "/");
            } else {
                if (current_path[strlen(current_path) - 1] != '/') {
                    strcat(current_path, "/");
                }
                strcat(current_path, name);
            }

            /* 检查是否匹配 */
            if (strcmp(current_path, path) == 0) {
                return (uint8_t *)p - (uint8_t *)g_fdt_ctx.dt_struct - 4;
            }

            depth++;
            p = (uint32_t *)ALIGN_UP((uintptr_t)p + name_len + 1, 4);
        } break;

        case FDT_END_NODE:
            depth--;
            if (depth < 0) {
                return -1;
            }
            /* 更新current_path，移除最后一个节点 */
            {
                char *last_slash = strrchr(current_path, '/');
                if (last_slash && last_slash != current_path) {
                    *last_slash = '\0';
                }
            }
            break;

        case FDT_PROP: {
            struct fdt_property *prop = (struct fdt_property *)p;
            uint32_t len = fdt32_to_cpu(prop->len);
            p = (uint32_t *)ALIGN_UP(
                (uintptr_t)p + sizeof(struct fdt_property) + len, 4);
        } break;

        case FDT_NOP:
            break;

        case FDT_END:
            return -1;

        default:
            return -1;
        }
    }

    return -1;
}

/**
 * 获取节点的属性
 * @param node_offset 节点偏移
 * @param prop_name 属性名
 * @param len 返回属性长度
 * @return 属性值指针，失败返回NULL
 */
const void *fdt_get_property(int node_offset, const char *prop_name, int *len) {
    uint32_t *p = (uint32_t *)((uint8_t *)g_fdt_ctx.dt_struct + node_offset);
    uint32_t tag;
    int depth = 0;

    /* 跳过BEGIN_NODE标记和节点名 */
    tag = fdt32_to_cpu(*p++);
    if (tag != FDT_BEGIN_NODE) {
        return NULL;
    }

    const char *name = (const char *)p;
    p = (uint32_t *)ALIGN_UP((uintptr_t)p + strlen(name) + 1, 4);

    /* 查找属性 */
    while (1) {
        tag = fdt32_to_cpu(*p++);

        switch (tag) {
        case FDT_PROP: {
            struct fdt_property *prop = (struct fdt_property *)p;
            const char *pname = fdt_get_property_name(prop->nameoff);

            if (strcmp(pname, prop_name) == 0) {
                if (len) {
                    *len = fdt32_to_cpu(prop->len);
                }
                return (uint8_t *)p + sizeof(struct fdt_property);
            }

            uint32_t plen = fdt32_to_cpu(prop->len);
            p = (uint32_t *)ALIGN_UP(
                (uintptr_t)p + sizeof(struct fdt_property) + plen, 4);
        } break;

        case FDT_BEGIN_NODE:
            /* 进入子节点，不再搜索 */
            return NULL;

        case FDT_END_NODE:
            return NULL;

        case FDT_NOP:
            break;

        case FDT_END:
            return NULL;

        default:
            return NULL;
        }
    }

    return NULL;
}

/**
 * 获取32位整数属性值
 */
int fdt_get_property_u32(int node_offset, const char *prop_name,
                         uint32_t *value) {
    int len;
    const uint32_t *prop = fdt_get_property(node_offset, prop_name, &len);

    if (!prop || len != sizeof(uint32_t)) {
        return -1;
    }

    *value = fdt32_to_cpu(*prop);
    return 0;
}

/**
 * 获取64位整数属性值
 */
int fdt_get_property_u64(int node_offset, const char *prop_name,
                         uint64_t *value) {
    int len;
    const uint64_t *prop = fdt_get_property(node_offset, prop_name, &len);

    if (!prop || len != sizeof(uint64_t)) {
        return -1;
    }

    *value = fdt64_to_cpu(*prop);
    return 0;
}

/**
 * 获取字符串属性值
 */
const char *fdt_get_property_string(int node_offset, const char *prop_name) {
    int len;
    const char *prop = fdt_get_property(node_offset, prop_name, &len);

    if (!prop || len <= 0) {
        return NULL;
    }

    /* 确保字符串以null结尾 */
    if (prop[len - 1] != '\0') {
        return NULL;
    }

    return prop;
}

/**
 * 遍历所有设备树节点（示例回调）
 */
void fdt_walk_nodes(fdt_node_callback callback) {
    uint32_t *p = (uint32_t *)g_fdt_ctx.dt_struct;
    int depth = 0;
    char path_stack[10][128]; /* 路径栈 */
    char current_path[256];

    strcpy(path_stack[0], "");

    while (1) {
        uint32_t tag = fdt32_to_cpu(*p);

        switch (tag) {
        case FDT_BEGIN_NODE: {
            int offset = (uint8_t *)p - (uint8_t *)g_fdt_ctx.dt_struct;
            p++;
            const char *name = (const char *)p;

            /* 构建路径 */
            if (depth == 0) {
                strcpy(current_path, "/");
                strcpy(path_stack[depth], "");
            } else {
                strcpy(current_path, "");
                for (int i = 0; i < depth; i++) {
                    if (strlen(path_stack[i]) > 0) {
                        strcat(current_path, "/");
                        strcat(current_path, path_stack[i]);
                    }
                }
                strcat(current_path, "/");
                strcat(current_path, name);
            }

            /* 调用回调 */
            if (callback) {
                callback(current_path, offset, depth);
            }

            strcpy(path_stack[depth], name);
            depth++;
            p = (uint32_t *)ALIGN_UP((uintptr_t)p + strlen(name) + 1, 4);
        } break;

        case FDT_END_NODE:
            depth--;
            if (depth < 0) {
                return;
            }
            p++;
            break;

        case FDT_PROP:
            p++;
            {
                struct fdt_property *prop = (struct fdt_property *)p;
                uint32_t len = fdt32_to_cpu(prop->len);
                p = (uint32_t *)ALIGN_UP(
                    (uintptr_t)p + sizeof(struct fdt_property) + len, 4);
            }
            break;

        case FDT_NOP:
            p++;
            break;

        case FDT_END:
            return;

        default:
            return;
        }
    }
}
