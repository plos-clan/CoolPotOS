#include "kconfig.h"
#include "krlibc.h"
#include "mem/slub.h"
#include "term/kprint.h"
#include "bootarg.h"
#include "klog.h"

/*
 * CoolPotOS 内核配置管理实现
 */

static struct llist_header g_config_list;
static spin_t g_config_lock = SPIN_INIT;
static bool g_config_init = false;

void kconfig_init(void) {
    llist_init_head(&g_config_list);
    g_config_init = true;

    /* 注册默认内核配置项 */
    kconfig_register("kernel.loglevel",     KCONFIG_TYPE_INT,   "1",  "日志级别 (0=DEBUG,1=INFO,2=WARN,3=ERROR,4=FATAL)");
    kconfig_register("kernel.serial_log",   KCONFIG_TYPE_BOOL,  "1",  "启用串口日志输出");
    kconfig_register("kernel.max_tasks",    KCONFIG_TYPE_UINT,  "256","最大任务数");
    kconfig_register("kernel.time_slice",   KCONFIG_TYPE_UINT,  "10", "调度时间片 (ms)");
    kconfig_register("kernel.stack_pages",  KCONFIG_TYPE_UINT,  "4",  "每任务内核栈页数");
    kconfig_register("vfs.max_fd",          KCONFIG_TYPE_UINT,  "256","最大文件描述符数");
    kconfig_register("vfs.max_path",        KCONFIG_TYPE_UINT,  "256","最大路径长度");
    kconfig_register("mem.slub_pool_size",  KCONFIG_TYPE_UINT,  "2M", "SLUB 分配器池大小");
    kconfig_register("debug.stack_canary",  KCONFIG_TYPE_BOOL,  "1",  "启用栈保护 (canary)");
    kconfig_register("debug.panic_on_oops", KCONFIG_TYPE_BOOL,  "1",  "Oops 后触发 panic");

    klog_info("KCONFIG", "Configuration system initialized (%d entries)", 10);
}

int kconfig_register(const char *key, kconfig_type_t type,
                     const char *default_val, const char *desc) {
    if (!key || !default_val || !g_config_init) return -EINVAL;

    spin_lock(g_config_lock);
    /* 检查是否已存在 */
    kconfig_entry_t *entry, *tmp;
    llist_for_each(entry, tmp, &g_config_list, node) {
        if (strcmp(entry->key, key) == 0) {
            spin_unlock(g_config_lock);
            return -EEXIST;
        }
    }

    entry = calloc(1, sizeof(kconfig_entry_t));
    if (!entry) {
        spin_unlock(g_config_lock);
        return -ENOMEM;
    }

    strncpy(entry->key, key, KCONFIG_KEY_MAX - 1);
    entry->key[KCONFIG_KEY_MAX - 1] = '\0';
    strncpy(entry->value, default_val, KCONFIG_VAL_MAX - 1);
    entry->value[KCONFIG_VAL_MAX - 1] = '\0';
    strncpy(entry->default_val, default_val, KCONFIG_VAL_MAX - 1);
    entry->default_val[KCONFIG_VAL_MAX - 1] = '\0';
    if (desc) {
        strncpy(entry->description, desc, sizeof(entry->description) - 1);
        entry->description[sizeof(entry->description) - 1] = '\0';
    }
    entry->type = type;
    entry->is_set = false;

    llist_append(&g_config_list, &entry->node);
    spin_unlock(g_config_lock);
    return 0;
}

const char *kconfig_get(const char *key) {
    if (!key) return NULL;
    spin_lock(g_config_lock);
    kconfig_entry_t *entry, *tmp;
    llist_for_each(entry, tmp, &g_config_list, node) {
        if (strcmp(entry->key, key) == 0) {
            spin_unlock(g_config_lock);
            return entry->value;
        }
    }
    spin_unlock(g_config_lock);
    return NULL;
}

int kconfig_set(const char *key, const char *value) {
    if (!key || !value) return -EINVAL;
    spin_lock(g_config_lock);
    kconfig_entry_t *entry, *tmp;
    llist_for_each(entry, tmp, &g_config_list, node) {
        if (strcmp(entry->key, key) == 0) {
            strncpy(entry->value, value, KCONFIG_VAL_MAX - 1);
            entry->value[KCONFIG_VAL_MAX - 1] = '\0';
            entry->is_set = true;
            spin_unlock(g_config_lock);
            return 0;
        }
    }
    spin_unlock(g_config_lock);
    return -ENOENT;
}

int64_t kconfig_get_int(const char *key) {
    const char *val = kconfig_get(key);
    if (!val) return 0;

    int64_t result = 0;
    bool negative = false;
    const char *p = val;

    if (*p == '-') { negative = true; p++; }
    while (*p >= '0' && *p <= '9') {
        result = result * 10 + (*p - '0');
        p++;
    }
    return negative ? -result : result;
}

uint64_t kconfig_get_uint(const char *key) {
    const char *val = kconfig_get(key);
    if (!val) return 0;

    /* 支持后缀 K/M/G */
    uint64_t result = 0;
    while (*val >= '0' && *val <= '9') {
        result = result * 10 + (*val - '0');
        val++;
    }
    if (*val == 'K' || *val == 'k') result *= 1024ULL;
    else if (*val == 'M' || *val == 'm') result *= 1024ULL * 1024ULL;
    else if (*val == 'G' || *val == 'g') result *= 1024ULL * 1024ULL * 1024ULL;

    return result;
}

bool kconfig_get_bool(const char *key) {
    const char *val = kconfig_get(key);
    if (!val) return false;
    return (val[0] == '1' || val[0] == 'y' || val[0] == 'Y' ||
            val[0] == 't' || val[0] == 'T');
}

int kconfig_load_bootargs(void) {
    /* 将启动参数作为配置覆盖 */
    kconfig_entry_t *entry, *tmp;
    int loaded = 0;

    spin_lock(g_config_lock);
    llist_for_each(entry, tmp, &g_config_list, node) {
        const char *boot_val = boot_get_cmdline_param(entry->key);
        if (boot_val && boot_val[0]) {
            strncpy(entry->value, boot_val, KCONFIG_VAL_MAX - 1);
            entry->value[KCONFIG_VAL_MAX - 1] = '\0';
            entry->is_set = true;
            loaded++;
        }
    }
    spin_unlock(g_config_lock);

    klog_info("KCONFIG", "Loaded %d config overrides from boot args", loaded);
    return loaded;
}

void kconfig_list_all(void) {
    spin_lock(g_config_lock);
    kconfig_entry_t *entry, *tmp;
    printk("=== Kernel Configuration ===\n");
    printk("%-30s %-8s %-20s %s\n", "KEY", "TYPE", "VALUE", "DEFAULT");
    printk("--------------------------------------------------------------\n");
    llist_for_each(entry, tmp, &g_config_list, node) {
        const char *type_str = "?";
        switch (entry->type) {
        case KCONFIG_TYPE_STRING: type_str = "STRING"; break;
        case KCONFIG_TYPE_INT:    type_str = "INT";    break;
        case KCONFIG_TYPE_BOOL:   type_str = "BOOL";   break;
        case KCONFIG_TYPE_UINT:   type_str = "UINT";   break;
        }
        printk("%-30s %-8s %-20s %s\n",
               entry->key, type_str, entry->value, entry->default_val);
    }
    spin_unlock(g_config_lock);
}

void kconfig_reset_defaults(void) {
    spin_lock(g_config_lock);
    kconfig_entry_t *entry, *tmp;
    llist_for_each(entry, tmp, &g_config_list, node) {
        strncpy(entry->value, entry->default_val, KCONFIG_VAL_MAX - 1);
        entry->value[KCONFIG_VAL_MAX - 1] = '\0';
        entry->is_set = false;
    }
    spin_unlock(g_config_lock);
    klog_info("KCONFIG", "All configuration reset to defaults");
}