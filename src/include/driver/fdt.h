#pragma once


/* FDT魔数 */
#define FDT_MAGIC 0xd00dfeed
#define FDT_VERSION 17
#define FDT_LAST_COMP_VERSION 16

/* FDT节点标记 */
#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

#include "types.h"

/* DTB头部结构 */
struct fdt_header {
    uint32_t magic;             /* 魔数 0xd00dfeed */
    uint32_t totalsize;         /* DTB总大小 */
    uint32_t off_dt_struct;     /* 结构块偏移 */
    uint32_t off_dt_strings;    /* 字符串块偏移 */
    uint32_t off_mem_rsvmap;    /* 内存保留映射偏移 */
    uint32_t version;           /* 版本号 */
    uint32_t last_comp_version; /* 最后兼容版本 */
    uint32_t boot_cpuid_phys;   /* 引导CPU ID */
    uint32_t size_dt_strings;   /* 字符串块大小 */
    uint32_t size_dt_struct;    /* 结构块大小 */
};

/* 内存保留区域 */
struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};

/* 属性结构 */
struct fdt_property {
    uint32_t len;     /* 属性值长度 */
    uint32_t nameoff; /* 属性名在字符串块中的偏移 */
};

/* FDT解析器上下文 */
struct fdt_context {
    void *dtb_base;                    /* DTB映射后的虚拟地址 */
    struct fdt_header *header;         /* DTB头部 */
    void *dt_struct;                   /* 结构块起始地址 */
    char *dt_strings;                  /* 字符串块起始地址 */
    struct fdt_reserve_entry *rsv_map; /* 保留内存映射 */
};

extern struct fdt_context g_fdt_ctx;

/* 大小端转换宏 */
#define fdt32_to_cpu(x) __builtin_bswap32(x)
#define fdt64_to_cpu(x) __builtin_bswap64(x)
#define cpu_to_fdt32(x) __builtin_bswap32(x)

/* 对齐宏 */
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

int fdt_init();
int fdt_find_node(const char *path);

const void *fdt_get_property(int node_offset, const char *prop_name, int *len);
int fdt_get_property_u32(int node_offset, const char *prop_name,
                                 uint32_t *value);
int fdt_get_property_u64(int node_offset, const char *prop_name,
                                 uint64_t *value);
const char *fdt_get_property_string(int node_offset, const char *prop_name);

typedef void (*fdt_node_callback)(const char *path, int offset, int depth);

void fdt_walk_nodes(fdt_node_callback callback);
