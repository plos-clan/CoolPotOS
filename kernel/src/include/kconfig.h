#pragma once

#include "types.h"
#include "lock.h"

/*
 * CoolPotOS 内核配置管理系统
 * 支持运行时键值配置、默认值、类型校验、持久化
 */

#define KCONFIG_KEY_MAX   64    /* 配置键最大长度 */
#define KCONFIG_VAL_MAX   256   /* 配置值最大长度 */
#define KCONFIG_MAX_ENTRIES 64  /* 最大配置条目数 */

/* 配置值类型 */
typedef enum {
    KCONFIG_TYPE_STRING = 0,  /* 字符串 */
    KCONFIG_TYPE_INT    = 1,  /* 整数 */
    KCONFIG_TYPE_BOOL   = 2,  /* 布尔值 */
    KCONFIG_TYPE_UINT   = 3,  /* 无符号整数 */
} kconfig_type_t;

/* 配置条目 */
typedef struct kconfig_entry {
    char            key[KCONFIG_KEY_MAX];
    char            value[KCONFIG_VAL_MAX];
    char            default_val[KCONFIG_VAL_MAX];
    char            description[128];
    kconfig_type_t  type;
    bool            is_set;         /* 是否已被设置 */
    struct llist_header node;
} kconfig_entry_t;

/* ---- API ---- */
void kconfig_init(void);

/* 注册配置项 (带默认值和描述) */
int kconfig_register(const char *key, kconfig_type_t type,
                     const char *default_val, const char *desc);

/* 获取/设置配置值 */
const char *kconfig_get(const char *key);
int kconfig_set(const char *key, const char *value);

/* 类型化获取 */
int64_t  kconfig_get_int(const char *key);
uint64_t kconfig_get_uint(const char *key);
bool     kconfig_get_bool(const char *key);

/* 从启动参数加载配置 */
int kconfig_load_bootargs(void);

/* 列出所有配置 */
void kconfig_list_all(void);

/* 重置为默认值 */
void kconfig_reset_defaults(void);