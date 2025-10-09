#include "dlinker.h"
#include "elf_util.h"
#include "klog.h"
#include "kprint.h"
#include "limine.h"
#include "page.h"
#include "sprintf.h"

uint64_t kernel_modules_load_offset = 0;

extern dlfunc_t __ksymtab_start[]; // .ksymtab section
extern dlfunc_t __ksymtab_end[];
size_t          dlfunc_count = 0;

char fmt_buf[4096];

void *resolve_symbol(Elf64_Sym *symtab, uint32_t sym_idx) {
    return (void *)symtab[sym_idx].st_value;
}

bool handle_relocations(Elf64_Rela *rela_start, Elf64_Sym *symtab, char *strtab, size_t jmprel_sz,
                        uint64_t offset) {
    Elf64_Rela *rela_plt   = rela_start;
    size_t      rela_count = jmprel_sz / sizeof(Elf64_Rela);

    for (size_t i = 0; i < rela_count; i++) {
        Elf64_Rela *rela        = &rela_plt[i];
        Elf64_Sym  *sym         = &symtab[ELF64_R_SYM(rela->r_info)];
        char       *sym_name    = &strtab[sym->st_name];
        dlfunc_t   *func        = find_func(sym_name);
        uint64_t   *target_addr = (uint64_t *)(rela->r_offset + offset);
        if (func != NULL) {
            *target_addr = (uint64_t)func->addr;
        } else {
            logkf("Failed relocating %s at %p\n", sym_name, rela->r_offset + offset);
            return false;
        }
    }
    return true;
}

void *find_symbol_address(const char *symbol_name, Elf64_Ehdr *ehdr, uint64_t offset) {
    if (symbol_name == NULL || ehdr == NULL) return NULL;

    Elf64_Sym *symtab = NULL;
    char      *strtab = NULL;

    Elf64_Shdr *shdrs    = (Elf64_Shdr *)((char *)ehdr + ehdr->e_shoff);
    char       *shstrtab = (char *)ehdr + shdrs[ehdr->e_shstrndx].sh_offset;

    size_t symtabsz = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB || shdrs[i].sh_type == SHT_DYNSYM) {
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
        if (strcmp(symbol_name, sym_name) == 0) {
            if (sym->st_shndx == SHN_UNDEF) {
                logkf("Symbol %s is undefined.\n", sym_name);
                return NULL;
            }
            void *addr = (void *)sym->st_value + offset;
            return addr;
        }
    }
    logkf("Cannot find symbol %s in ELF file.\n", symbol_name);
    return NULL;
}

dlinit_t load_dynamic(kernel_mode_t *kmod,Elf64_Phdr *phdrs, Elf64_Ehdr *ehdr, uint64_t offset) {
    Elf64_Dyn *dyn_entry = NULL;
    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dyn_entry = (Elf64_Dyn *)(phdrs[i].p_vaddr);
            break;
        }
    }
    if (dyn_entry == NULL) {
        logkf("Dynamic section not found.\n");
        return NULL;
    }
    uint64_t addr_dyn = ((uint64_t)dyn_entry) + offset;
    dyn_entry         = (Elf64_Dyn *)addr_dyn;

    Elf64_Sym  *symtab = NULL;
    char       *strtab = NULL;
    Elf64_Rela *rel    = NULL;
    Elf64_Rela *jmprel = NULL;
    size_t      relsz = 0, symtabsz = 0, jmprel_sz = 0;

    while (dyn_entry->d_tag != DT_NULL) {
        switch (dyn_entry->d_tag) {
        case DT_SYMTAB:
            uint64_t symtab_addr = dyn_entry->d_un.d_ptr + offset;
            symtab               = (Elf64_Sym *)symtab_addr;
            break;
        case DT_STRTAB: strtab = (char *)dyn_entry->d_un.d_ptr + offset; break;
        case DT_RELA:
            uint64_t rel_addr = dyn_entry->d_un.d_ptr + offset;
            rel               = (Elf64_Rela *)rel_addr;
            break;
        case DT_RELASZ: relsz = dyn_entry->d_un.d_val; break;
        case DT_JMPREL:
            uint64_t jmprel_addr = dyn_entry->d_un.d_ptr + offset;
            jmprel               = (Elf64_Rela *)jmprel_addr;
            break;
        case DT_SYMENT: symtabsz = dyn_entry->d_un.d_val; break;
        case DT_PLTRELSZ: jmprel_sz = dyn_entry->d_un.d_val; break;
        case DT_PLTGOT: /* 需要解析 PLT 表 */ break;
        }
        dyn_entry++;
    }

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
    if (!handle_relocations(jmprel, symtab, strtab, jmprel_sz, offset)) {
        logkf("Failed to handle relocations.\n");
        return NULL;
    }

    void *entry = find_symbol_address("dlmain", ehdr, offset);
    if (entry == NULL) { entry = find_symbol_address("_dlmain", ehdr, offset); }
    kmod->entry = entry;

    void *tentry = find_symbol_address("dlstart", ehdr, offset);
    if (tentry == NULL) { tentry = find_symbol_address("_dlstart", ehdr, offset); }
    kmod->task_entry = tentry;

    dlinit_t dlinit_func = (dlinit_t)entry;
    return dlinit_func;
}

void dlinker_load(kernel_mode_t *kmod,cp_module_t *module) {
    if (module == NULL) return;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)module->data;

    Elf64_Phdr phdr[12];
    if (ehdr->e_phnum > sizeof(phdr) / sizeof(phdr[0]) || ehdr->e_phnum < 1) {
        logkf("ELF file has wrong number of program headers");
        return;
    }

    if (ehdr->e_type != ET_DYN) {
        logkf("ELF file is not a dynamic library.\n");
        return;
    }

    uint64_t load_size = 0;

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    if (!mmap_phdr_segment(ehdr, phdrs, get_current_directory(), false,
                           KERNEL_MOD_SPACE_START + kernel_modules_load_offset, NULL, &load_size)) {
        logkf("Cannot mmap elf segment.\n");
        return;
    }

    dlinit_t dlinit =
        load_dynamic(kmod,phdrs, ehdr, KERNEL_MOD_SPACE_START + kernel_modules_load_offset);
    if (dlinit == NULL) {
        dlinit = (dlinit_t)ehdr->e_entry;
        if (dlinit == NULL) {
            logkf("Cannot find dlinit function.\n");
            return;
        }
    }

    kinfo("Loaded module %s at %#018lx", module->module_name,
          KERNEL_MOD_SPACE_START + kernel_modules_load_offset);
    logkf("kmod: loaded module %s at %#018lx\n", module->module_name, KERNEL_MOD_SPACE_START + kernel_modules_load_offset);
    int ret = dlinit();

    (void)ret;

    kernel_modules_load_offset += (load_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

dlfunc_t *find_func(const char *name) {
    for (size_t i = 0; i < dlfunc_count; i++) {
        dlfunc_t *entry = &__ksymtab_start[i];
        if (strcmp(entry->name, name) == 0) { return entry; }
    }
    return NULL;
}

void find_kernel_symbol() {
    dlfunc_count = __ksymtab_end - __ksymtab_start;
    for (size_t i = 0; i < dlfunc_count; i++) {
        dlfunc_t *entry = &__ksymtab_start[i];
    }
}

void dlinker_init() {
    find_kernel_symbol();
}
