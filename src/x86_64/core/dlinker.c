#include "dlinker.h"
#include "alloc.h"
#include "kprint.h"
#include "krlibc.h"
#include "sprintf.h"
#include "timer.h"

dlfunc_t *func_table;
int       funs_num = 0;

static dlfunc_t *find_func(const char *name) {
    for (int i = 0; i < funs_num; i++) {
        if (strcmp(func_table[i].name, name) == 0) { return &func_table[i]; }
    }
    return NULL;
}

void *resolve_symbol(Elf64_Sym *symtab, char *strtab, uint32_t sym_idx) {
    return (void *)symtab[sym_idx].st_value;
}

bool handle_relocations(Elf64_Rela *rela_start, Elf64_Sym *symtab, char *strtab) {
    Elf64_Rela *rela_entry = rela_start;
    while (rela_entry->r_offset != 0) {
        Elf64_Sym *sym      = &symtab[ELF64_R_SYM(rela_entry->r_info)];
        char      *sym_name = &strtab[sym->st_name];
        dlfunc_t  *func     = find_func(sym_name);
        if (func != NULL) {
            *(void **)rela_entry->r_offset = func->addr;
        } else {
            kwarn("Failed relocating %s at %p", sym_name, rela_entry->r_offset);
            return false;
        }
        rela_entry++;
    }
    return true;
}

void *find_symbol_address(const char *symbol_name, Elf64_Sym *symtab, char *strtab,
                          size_t symtabsz) {
    for (size_t i = 0; i < symtabsz; i++) {
        Elf64_Sym *sym      = &symtab[i];
        char      *sym_name = &strtab[sym->st_name];
        if (strcmp(symbol_name, sym_name) == 0) {
            void *addr = (void *)sym->st_value;
            return addr;
        }
    }
    return NULL;
}

dlmain_t load_dynamic(Elf64_Phdr *phdrs, Elf64_Ehdr *ehdr) {
    Elf64_Dyn *dyn_entry = NULL;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dyn_entry = (Elf64_Dyn *)(phdrs[i].p_vaddr);
            break;
        }
    }
    if (dyn_entry == NULL) return NULL;

    Elf64_Sym  *symtab = NULL;
    char       *strtab = NULL;
    Elf64_Rela *rel    = NULL;
    Elf64_Rela *jmprel = NULL;
    size_t      relsz = 0, jmprelsz = 0, symtabsz = 0;

    while (dyn_entry->d_tag != DT_NULL) {
        switch (dyn_entry->d_tag) {
        case DT_SYMTAB: symtab = (Elf64_Sym *)dyn_entry->d_un.d_ptr; break;
        case DT_STRTAB: strtab = (char *)dyn_entry->d_un.d_ptr; break;
        case DT_RELA: rel = (Elf64_Rela *)dyn_entry->d_un.d_ptr; break;
        case DT_RELASZ: relsz = dyn_entry->d_un.d_val; break;
        case DT_JMPREL: jmprel = (Elf64_Rela *)dyn_entry->d_un.d_ptr; break;
        case DT_SYMENT: symtabsz = dyn_entry->d_un.d_val; break;
        case DT_PLTGOT: /* 需要解析 PLT 表 */ break;
        }
        dyn_entry++;
    }

    for (size_t i = 0; i < relsz / sizeof(Elf64_Rela); i++) {
        Elf64_Rela *r          = &rel[i];
        uint64_t   *reloc_addr = (uint64_t *)r->r_offset;
        uint32_t    sym_idx    = ELF64_R_SYM(r->r_info);
        uint32_t    type       = ELF64_R_TYPE(r->r_info);

        if (type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, strtab, sym_idx);
        } else if (type == R_X86_64_RELATIVE) {
            *reloc_addr = (uint64_t)((char *)ehdr + r->r_addend);
        }
    }
    if (!handle_relocations(jmprel, symtab, strtab)) { return NULL; }

    void *entry = find_symbol_address("dlmain", symtab, strtab, symtabsz);
    if (entry == NULL) {
        kwarn("Cannot find dynamic library main function.");
        return NULL;
    }

    dlmain_t dlmain_func = (dlmain_t)entry;
    return dlmain_func;
}

void dlinker_load(cp_module_t *module) {
    if (module == NULL) return;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)module->data;
    if (!elf_test_head(ehdr)) {
        kwarn("No elf file.");
        return;
    }

    Elf64_Phdr phdr[12];
    if (ehdr->e_phnum > sizeof(phdr) / sizeof(phdr[0]) || ehdr->e_phnum < 1) {
        kwarn("ELF file has wrong number of program headers");
        return;
    }

    if (ehdr->e_type != ET_DYN) {
        kwarn("ELF file is not a dynamic library.");
        return;
    }

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    if (!mmap_phdr_segment(ehdr, phdrs, get_kernel_pagedir(), false)) {
        kwarn("Cannot mmap elf segment.");
        return;
    }

    dlmain_t dlmain = load_dynamic(phdrs, ehdr);
    if (dlmain == NULL) {
        kwarn("Cannot load dynamic section.");
        return;
    }
    int ret = dlmain();
    kinfo("Kernel model load done! Return code:%d.", ret);
}

void register_library_func(const char *name, void *func) {
    if (name == NULL || func == NULL) {
        kwarn("Invalid function name or address.");
        return;
    }
    if (funs_num >= 256) {
        kwarn("Too many functions in dynamic library.");
        return;
    }
    func_table[funs_num].name = malloc(strlen(name) + 1);
    memset(func_table[funs_num].name, 0, strlen(name) + 1);
    strcpy(func_table[funs_num].name, name);
    func_table[funs_num].addr = func;
    funs_num++;
}

void dlinker_init() {
    func_table = malloc(sizeof(dlfunc_t) * 256);
    funs_num   = 0;

    register_library_func("printf", cp_printf);
    register_library_func("malloc", malloc);
    register_library_func("free", free);
    register_library_func("register_library_func", register_library_func);
    register_library_func("get_hour\0", get_hour);
    register_library_func("get_min", get_min);

    kinfo("Dynamic linker initialized.");
}
