#include "exec/dlinker.h"
#include "errno.h"
#include "lib/sprintf.h"
#include "limine.h"
#include "module.h"
#include "term/klog.h"

#define EXPORT_SYMBOL(mode, FUNC) dlfunc_register(mode, #FUNC, (void *)(FUNC))

#define EXPORT_INITIAL_CAPACITY 4
#define EXPORT_GROWTH_FACTOR    2

cow_arraylist *kmod_lists;
uint64_t       kernel_modules_load_offset = 0;

dlfunc_t *find_func(char *name) {
    kernel_mode_t *kmod = NULL;
    cow_foreach(kmod_lists, kmod) {
        for (size_t i = 0; i < kmod->export_count; i++) {
            dlfunc_t *func = kmod->export_funcs[i];
            if (strcmp(func->name, name) == 0) return func;
        }
    }
    return NULL;
}

dlfunc_t *find_func_mode(kernel_mode_t *mode, char *name) {
    kernel_mode_t *kmod = mode;
    for (size_t i = 0; i < kmod->export_count; i++) {
        dlfunc_t *func = kmod->export_funcs[i];
        if (strcmp(func->name, name) == 0) return func;
    }
    return NULL;
}

void dlfunc_register(kernel_mode_t *mode, char *name, void *func) {
    if (mode == NULL || name == NULL || func == NULL) {
        printk("Error: Invalid arguments to dlfunc_register.\n");
        return;
    }

    if (mode->export_count >= mode->lists_index) {
        size_t     new_capacity = (mode->lists_index == 0) ? EXPORT_INITIAL_CAPACITY
                                                           : mode->lists_index * EXPORT_GROWTH_FACTOR;
        dlfunc_t **new_funcs =
            (dlfunc_t **)realloc(mode->export_funcs, new_capacity * sizeof(dlfunc_t *));

        if (new_funcs == NULL) {
            printk("Error: Memory allocation failed for export_funcs.\n");
            return;
        }

        mode->export_funcs = new_funcs;
        mode->lists_index  = new_capacity;
    }

    dlfunc_t *new_entry = (dlfunc_t *)malloc(sizeof(dlfunc_t));
    if (new_entry == NULL) {
        printk("Error: Memory allocation failed for dlfunc_t entry.\n");
        return;
    }

    new_entry->name = strdup(name);
    new_entry->addr = func;

    mode->export_funcs[mode->export_count] = new_entry;
    mode->export_count++;
}

void *resolve_symbol(Elf64_Sym *symtab, uint32_t sym_idx) {
    return (void *)symtab[sym_idx].st_value;
}

bool handle_relocations(Elf64_Rela *rela_start, Elf64_Sym *symtab, char *strtab, size_t jmprel_sz,
                        uint64_t offset) {
    Elf64_Rela *rela_plt   = rela_start;
    size_t      rela_count = jmprel_sz / sizeof(Elf64_Rela);

    for (size_t i = 0; i < rela_count; i++) {
        Elf64_Rela *rela     = &rela_plt[i];
        Elf64_Sym  *sym      = &symtab[ELF64_R_SYM(rela->r_info)];
        char       *sym_name = &strtab[sym->st_name];
        uint64_t    bind     = ELF64_ST_BIND(sym->st_info);
        if (bind == STB_GLOBAL && sym->st_shndx == SHN_UNDEF) {
            dlfunc_t *func        = find_func(sym_name);
            uint64_t *target_addr = (uint64_t *)(rela->r_offset + offset);
            if (func != NULL) {
                *target_addr = (uint64_t)func->addr;
            } else {
                printk("Failed relocating %s at %p\n", sym_name, rela->r_offset + offset);
            }
        }
    }
    return true;
}

void export_symbol(kernel_mode_t *mode, Elf64_Ehdr *ehdr, uint64_t offset) {
    if (ehdr == NULL) return;

    Elf64_Sym *symtab = NULL;
    char      *strtab = NULL;

    Elf64_Shdr *shdrs    = (Elf64_Shdr *)((char *)ehdr + ehdr->e_shoff);
    char       *shstrtab = (char *)ehdr + shdrs[ehdr->e_shstrndx].sh_offset;

    size_t symtabsz = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab   = (Elf64_Sym *)((char *)ehdr + shdrs[i].sh_offset);
            symtabsz = shdrs[i].sh_size;
            strtab   = (char *)ehdr + shdrs[shdrs[i].sh_link].sh_offset;
            break;
        }
    }

    size_t num_symbols = symtabsz / sizeof(Elf64_Sym);

    for (size_t i = 0; i < num_symbols; i++) {
        Elf64_Sym *sym      = &symtab[i];
        char      *sym_name = &strtab[sym->st_name];
        uint64_t   bind     = ELF64_ST_BIND(sym->st_info);
        uint64_t   type     = ELF64_ST_TYPE(sym->st_info);
        if (sym->st_shndx == SHN_UNDEF) continue;
        if (type == STT_FUNC && (bind == STB_GLOBAL)) {
            dlfunc_register(mode, sym_name, (void *)(offset + sym->st_value));
        }
    }
}

dlinit_t load_dynamic(kernel_mode_t *mode, Elf64_Phdr *phdrs, Elf64_Ehdr *ehdr, uint64_t offset) {
    Elf64_Dyn *dyn_entry = NULL;
    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dyn_entry = (Elf64_Dyn *)(phdrs[i].p_vaddr);
            break;
        }
    }
    if (dyn_entry == NULL) {
        printk("Dynamic section not found.\n");
        return NULL;
    }
    uint64_t addr_dyn = ((uint64_t)dyn_entry) + offset;
    dyn_entry         = (Elf64_Dyn *)addr_dyn;

    Elf64_Sym  *symtab = NULL;
    char       *strtab = NULL;
    Elf64_Rela *rel    = NULL;
    Elf64_Rela *jmprel = NULL;
    size_t      relsz = 0, jmprel_sz = 0;

    while (dyn_entry->d_tag != DT_NULL) {
        switch (dyn_entry->d_tag) {
        case DT_SYMTAB:;
            uint64_t symtab_addr = dyn_entry->d_un.d_ptr + offset;
            symtab               = (Elf64_Sym *)symtab_addr;
            break;
        case DT_STRTAB: strtab = (char *)dyn_entry->d_un.d_ptr + offset; break;
        case DT_RELA:;
            uint64_t rel_addr = dyn_entry->d_un.d_ptr + offset;
            rel               = (Elf64_Rela *)rel_addr;
            break;
        case DT_RELASZ: relsz = dyn_entry->d_un.d_val; break;
        case DT_JMPREL:;
            uint64_t jmprel_addr = dyn_entry->d_un.d_ptr + offset;
            jmprel               = (Elf64_Rela *)jmprel_addr;
            break;
        case DT_PLTRELSZ: jmprel_sz = dyn_entry->d_un.d_val; break;
        case DT_PLTGOT: /* 需要解析 PLT 表 */ break;
        }
        dyn_entry++;
    }

#if defined(__x86_64__)
    for (size_t i = 0; i < relsz / sizeof(Elf64_Rela); i++) {
        Elf64_Rela *r          = &rel[i];
        uint64_t   *reloc_addr = (uint64_t *)(r->r_offset + offset);
        uint32_t    sym_idx    = ELF64_R_SYM(r->r_info);
        uint32_t    type       = ELF64_R_TYPE(r->r_info);

        if (type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx) + offset;
        } else if (type == R_X86_64_RELATIVE) {
            *reloc_addr = (uint64_t)(offset + r->r_addend);
        } else if (type == R_X86_64_64) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx) + r->r_addend + offset;
        }
    }
#elif defined(__aarch64__)
    for (size_t i = 0; i < relsz / sizeof(Elf64_Rela); i++) {
        Elf64_Rela *r          = &rel[i];
        uint64_t   *reloc_addr = (uint64_t *)(r->r_offset + offset);
        uint32_t    sym_idx    = ELF64_R_SYM(r->r_info);
        uint32_t    type       = ELF64_R_TYPE(r->r_info);

        if (type == R_AARCH64_JUMP26 || type == R_AARCH64_CALL26) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx) + offset;
        } else if (type == R_AARCH64_RELATIVE) {
            *reloc_addr = (uint64_t)(offset + r->r_addend);
        } else if (type == R_AARCH64_ABS64) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx) + r->r_addend + offset;
        }
    }
#elif defined(__riscv) && (__riscv_xlen == 64)
    for (size_t i = 0; i < relsz / sizeof(Elf64_Rela); i++) {
        Elf64_Rela *r          = &rel[i];
        uint64_t   *reloc_addr = (uint64_t *)(r->r_offset + offset);
        uint32_t    sym_idx    = ELF64_R_SYM(r->r_info);
        uint32_t    type       = ELF64_R_TYPE(r->r_info);

        if (type == R_RISCV_JUMP_SLOT) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx) + offset;
        } else if (type == R_RISCV_COPY) {
            memcpy(reloc_addr, (void *)((uint64_t)resolve_symbol(symtab, sym_idx) + offset),
                   symtab[sym_idx].st_size);
        } else if (type == R_RISCV_RELATIVE) {
            *reloc_addr = (uint64_t)(offset + r->r_addend);
        } else if (type == R_RISCV_64) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx) + r->r_addend + offset;
        }
    }
#endif

    if (!handle_relocations(jmprel, symtab, strtab, jmprel_sz, offset)) {
        printk("Failed to handle relocations.\n");
        return NULL;
    }

    export_symbol(mode, ehdr, offset);
    dlfunc_t *dlinit_func = (dlfunc_t *)find_func_mode(mode, "dlmain");
    if (dlinit_func == NULL) return NULL;
    dlfunc_t *dlstart = (dlfunc_t *)find_func_mode(mode, "dlstart");
    if (dlstart != NULL) mode->task_entry = dlstart->addr;
    return dlinit_func->addr;
}

void dlinker_load(kernel_mode_t *module) {
    if (module == NULL) return;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)module->data;
    if (!arch_elf_test_head(ehdr)) {
        printk("No elf file.\n");
        return;
    }

    if (ehdr->e_type != ET_DYN) {
        printk("ELF file is not a dynamic library.\n");
        return;
    }

    uint64_t load_size = 0;

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    if (!mmap_phdr_segment(ehdr, phdrs, get_kernel_pagedir(), false,
                           KERNEL_MODULES_SPACE_START + kernel_modules_load_offset, NULL,
                           &load_size)) {
        printk("Cannot mmap elf segment.\n");
        return;
    }

    dlinit_t dlinit =
        load_dynamic(module, phdrs, ehdr, KERNEL_MODULES_SPACE_START + kernel_modules_load_offset);
    if (dlinit == NULL) {
        printk("cannot load dynamic section.\n");
        return;
    }

    kinfo("Loaded module %s at %#018lx", module->name,
          KERNEL_MODULES_SPACE_START + kernel_modules_load_offset);

    module->entry_exit_code     = dlinit();
    kernel_modules_load_offset += (load_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static void cp_printk(const char *fmt, ...) {
    char    buf[4096] = {0};
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf, fmt, args);
    va_end(args);
    tty_t *tty_ = kernel_session;
    tty_->ops.write(tty_, buf, 0, strlen(buf));
    tty_->ops.flush(tty_);
}

static bool ends_with_km(const char *str) {
    size_t len = strlen(str);
    if (len < 3) return false;
    return strcmp(str + len - 3, ".km") == 0;
}

extern module_t boot_modules[256];
extern size_t   modules_count;

void load_all_kernel_module() {
    for (size_t i = 0; i < modules_count; i++) {
        if (ends_with_km(boot_modules[i].path)) {
            module_t      *mod  = &boot_modules[i];
            kernel_mode_t *kmod = calloc(1,sizeof(kernel_mode_t));
            kmod->name          = strdup(mod->name);
            kmod->data          = mod->data;
            kmod->data_len      = mod->size;
            dlinker_load(kmod);
            kmod->lists_index = cow_list_add(kmod_lists, kmod);
        }
    }
}

void start_all_kernel_module() {
    kernel_mode_t *kmod;
    cow_foreach(kmod_lists, kmod) {
        if (kmod->task_entry == NULL) continue;
        if (kmod->entry_exit_code & ERRNO_MASK) {
            logkf("kmod: cannot start mod(%s) - exit_code: %d\n", kmod->name,
                  kmod->entry_exit_code);
        } else {
            int ret = kmod->task_entry();
        }
    }
}

static inline void register_cp_kernel_lib(kernel_mode_t *kernel) {
    dlfunc_register(kernel, "printk", cp_printk);
    dlfunc_register(kernel, "memset", memset);
    dlfunc_register(kernel, "memmove", memmove);
    dlfunc_register(kernel, "memchr", memchr);
    dlfunc_register(kernel, "memcmp", memcmp);
    dlfunc_register(kernel, "memcpy", memcpy);
    dlfunc_register(kernel, "strnlen", strnlen);
    dlfunc_register(kernel, "strlen", strlen);
    dlfunc_register(kernel, "strcat", strcat);
    dlfunc_register(kernel, "strcpy", strcpy);
    dlfunc_register(kernel, "strncpy", strncpy);
    dlfunc_register(kernel, "strchrnul", strchrnul);
    dlfunc_register(kernel, "strncmp", strncmp);
    dlfunc_register(kernel, "strchr", strchr);
    dlfunc_register(kernel, "strcmp", strcmp);
    dlfunc_register(kernel, "strrchr", strrchr);
    dlfunc_register(kernel, "strtok", strtok);
    dlfunc_register(kernel, "strdup", strdup);
    dlfunc_register(kernel, "strndup", strndup);
    dlfunc_register(kernel, "strtol", strtol);
    dlfunc_register(kernel, "sprintf", sprintf);
    dlfunc_register(kernel, "snprintf", snprintf);
    dlfunc_register(kernel, "malloc", malloc);
    dlfunc_register(kernel, "free", free);
}

static inline void register_fs_subsystem_lib(kernel_mode_t *kernel){
    dlfunc_register(kernel,"vfs_mkdir", vfs_mkdir);
    dlfunc_register(kernel,"vfs_mkfile",vfs_mkfile);
    dlfunc_register(kernel,"vfs_regist",vfs_regist);
    dlfunc_register(kernel,"vfs_link",vfs_link);
    dlfunc_register(kernel,"vfs_symlink",vfs_symlink);
    dlfunc_register(kernel,"vfs_child_append",vfs_child_append);
    dlfunc_register(kernel,"vfs_node_alloc",vfs_node_alloc);
    dlfunc_register(kernel,"vfs_close",vfs_close);
    dlfunc_register(kernel,"vfs_free",vfs_free);
    dlfunc_register(kernel,"vfs_update",vfs_update);
    dlfunc_register(kernel,"vfs_open",vfs_open);
    dlfunc_register(kernel,"vfs_ioctl",vfs_ioctl);
    dlfunc_register(kernel,"vfs_readlink",vfs_readlink);
    dlfunc_register(kernel,"get_filesystem",get_filesystem);
    dlfunc_register(kernel,"get_filesystem_node",get_filesystem_node);
    dlfunc_register(kernel,"get_rootdir",get_rootdir);
    dlfunc_register(kernel,"vfs_get_fullpath",vfs_get_fullpath);
    dlfunc_register(kernel,"vfs_read",vfs_read);
    dlfunc_register(kernel,"vfs_write",vfs_write);
    dlfunc_register(kernel,"vfs_mount",vfs_mount);
    dlfunc_register(kernel,"vfs_unmount",vfs_unmount);
    dlfunc_register(kernel,"general_map",general_map);
}

void kmodule_init() {
    kmod_lists            = cow_list_create();
    kernel_mode_t *kernel = malloc(sizeof(kernel_mode_t));
    *kernel = (kernel_mode_t){};
    kernel->name          = strdup("kernel");
    register_cp_kernel_lib(kernel);
    register_fs_subsystem_lib(kernel);
    kernel->lists_index = cow_list_add(kmod_lists, kernel);
    load_all_kernel_module();
    kinfo("Load kernel module...");
}
