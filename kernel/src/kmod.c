#include "kmod.h"
#include "krlibc.h"
#include "mem/slub.h"
#include "term/kprint.h"

/* 全局模块列表 */
static struct llist_header g_kmod_list;
static spin_t g_kmod_lock = SPIN_INIT;

/* 全局符号表 */
static struct llist_header g_symbol_list;
static spin_t g_symbol_lock = SPIN_INIT;

void kmod_init(void) {
    llist_init_head(&g_kmod_list);
    llist_init_head(&g_symbol_list);
    kinfo("Kmod: module framework initialized");
}

int kmod_load(const char *path, kernel_module_t **out_mod) {
    (void)path;
    if (out_mod) *out_mod = NULL;
    /* TODO: 实现 ELF 加载逻辑 */
    return -ENOSYS;
}

int kmod_unload(kernel_module_t *mod) {
    if (!mod) return -EINVAL;
    if (mod->state != KMOD_LOADED) return -EBUSY;

    mod->state = KMOD_UNLOADING;
    if (mod->exit) mod->exit(mod);

    spin_lock(g_kmod_lock);
    llist_delete(&mod->node);
    spin_unlock(g_kmod_lock);

    mod->state = KMOD_UNLOADED;
    free(mod);
    return 0;
}

int kmod_register(kernel_module_t *mod) {
    if (!mod || !mod->info.name[0]) return -EINVAL;

    spin_lock(g_kmod_lock);
    mod->state = KMOD_LOADED;
    llist_append(&g_kmod_list, &mod->node);
    spin_unlock(g_kmod_lock);

    kinfo("Kmod: registered module '%s' v%s", mod->info.name, mod->info.version);
    return 0;
}

int kmod_unregister(kernel_module_t *mod) {
    return kmod_unload(mod);
}

kernel_module_t *kmod_find(const char *name) {
    if (!name) return NULL;
    spin_lock(g_kmod_lock);
    kernel_module_t *mod, *tmp;
    llist_for_each(mod, tmp, &g_kmod_list, node) {
        if (strcmp(mod->info.name, name) == 0) {
            spin_unlock(g_kmod_lock);
            return mod;
        }
    }
    spin_unlock(g_kmod_lock);
    return NULL;
}

int kmod_get_count(void) {
    spin_lock(g_kmod_lock);
    int count = 0;
    kernel_module_t *mod, *tmp;
    llist_for_each(mod, tmp, &g_kmod_list, node) count++;
    spin_unlock(g_kmod_lock);
    return count;
}

void kmod_list_all(void) {
    spin_lock(g_kmod_lock);
    kernel_module_t *mod, *tmp;
    printk("=== Loaded Modules ===\n");
    printk("NAME\t\tVERSION\t\tSTATE\n");
    llist_for_each(mod, tmp, &g_kmod_list, node) {
        const char *state_str = "?";
        switch (mod->state) {
        case KMOD_UNLOADED:  state_str = "UNLOADED"; break;
        case KMOD_LOADING:   state_str = "LOADING"; break;
        case KMOD_LOADED:    state_str = "LOADED"; break;
        case KMOD_UNLOADING: state_str = "UNLOADING"; break;
        case KMOD_ERROR:     state_str = "ERROR"; break;
        }
        printk("%s\t\t%s\t\t%s\n", mod->info.name, mod->info.version, state_str);
    }
    spin_unlock(g_kmod_lock);
}

/* 符号表管理 */
int kmod_export_symbol(const char *name, void *addr) {
    if (!name || !addr) return -EINVAL;

    kernel_symbol_t *sym = malloc(sizeof(kernel_symbol_t));
    if (!sym) return -ENOMEM;

    sym->name = strdup(name);
    sym->addr = addr;

    spin_lock(g_symbol_lock);
    llist_append(&g_symbol_list, &sym->node);
    spin_unlock(g_symbol_lock);

    return 0;
}

void *kmod_find_symbol(const char *name) {
    if (!name) return NULL;
    kernel_symbol_t *sym, *tmp;
    llist_for_each(sym, tmp, &g_symbol_list, node) {
        if (strcmp(sym->name, name) == 0) return sym->addr;
    }
    return NULL;
}

int kmod_remove_symbol(const char *name) {
    if (!name) return -EINVAL;
    kernel_symbol_t *sym, *tmp;
    llist_for_each(sym, tmp, &g_symbol_list, node) {
        if (strcmp(sym->name, name) == 0) {
            spin_lock(g_symbol_lock);
            llist_delete(&sym->node);
            spin_unlock(g_symbol_lock);
            free((void *)sym->name);
            free(sym);
            return 0;
        }
    }
    return -ENOENT;
}