#include "elf_util.h"
#include "klog.h"
#include "krlibc.h"

void segment_callback(struct ElfSegment segment) {
    page_directory_t *pg_dir = get_current_directory();
    for (size_t i = segment.address; i < segment.address + segment.size; i += PAGE_SIZE) {
        alloc_frame(get_page(i, 1, pg_dir), 0, 1);
    }
    memcpy((void *)segment.address, segment.data, segment.size);
}

uint32_t load_elf(Elf32_Ehdr *hdr, page_directory_t *dir) {
    Elf32_Phdr *phdr = (Elf32_Phdr *)((uint32_t)hdr + hdr->e_phoff);
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr->p_type == PT_LOAD) {
            logkf("elf seg: v:%08x p:%08x ms:%d fs:%d offset:%08x\n", phdr->p_vaddr, phdr->p_paddr,
                  phdr->p_memsz, phdr->p_filesz, phdr->p_offset);
            load_segment(phdr, dir, (void *)hdr);
        }
        phdr++;
    }
    return hdr->e_entry;
}

void load_segment(Elf32_Phdr *phdr, page_directory_t *dir, void *elf) {
    size_t hi = PADDING_UP(phdr->p_paddr + phdr->p_memsz, 0x1000);
    size_t lo = PADDING_DOWN(phdr->p_paddr, 0x1000);
    if ((phdr->p_flags & PF_R) && !(phdr->p_flags & PF_W)) {
        for (size_t i = lo; i < hi; i += 0x1000) {
            alloc_frame_line(get_page(i, 1, dir), i, 0, 0);
        }
    } else
        for (size_t i = lo; i < hi; i += 0x1000) {
            alloc_frame_line(get_page(i, 1, dir), i, 0, 1);
        }
    uint32_t p_vaddr  = (uint32_t)phdr->p_vaddr;
    uint32_t p_filesz = (uint32_t)phdr->p_filesz;
    uint32_t p_memsz  = (uint32_t)phdr->p_memsz;
    memcpy((void *)phdr->p_vaddr, elf + phdr->p_offset, phdr->p_memsz);

    if (p_memsz > p_filesz) { // 这个是bss段
        memset((void *)(p_vaddr + p_filesz), 0, p_memsz - p_filesz);
    }
}

uint32_t elf_load(size_t elf_size, uint8_t *elf_data) {
    struct ElfParseResult result = parse_elf(elf_data, elf_size, segment_callback);
    switch (result.tag) {
    case EntryPoint: return result.entry_point;
    case InvalidElfData: printk("Invalid ELF data.\n"); break;
    case ElfContainsNoSegments: printk("ELF contains no segments.\n"); break;
    case FailedToGetSegmentData: printk("Failed to get segment data.\n"); break;
    case AllocFunctionNotProvided: printk("Allocation function not provided.\n"); break;
    default: printk("Unknown error.\n"); break;
    }
    return 0;
}

static char *get_shstr(Elf32_Ehdr *elf_h, Elf32_Shdr *shstr_hdr, Elf32_Word index) {
    char *data = (char *)((uint32_t)elf_h + shstr_hdr->sh_offset);
    return data + index;
}

bool check_elf_magic(Elf32_Ehdr *hdr) {
    if (hdr->e_ident[0] != ELFMAG0) return false;
    if (hdr->e_ident[1] != ELFMAG1) return false;
    if (hdr->e_ident[2] != ELFMAG2) return false;
    if (hdr->e_ident[3] != ELFMAG3) return false;
    return true;
}

Elf32_Half find_section_index(char *name, Elf32_Ehdr *elf_h) {
    int         i;
    Elf32_Shdr *sect_hdr  = (Elf32_Shdr *)((uint32_t)elf_h + elf_h->e_shoff);
    Elf32_Shdr *shstr_hdr = &sect_hdr[elf_h->e_shstrndx];
    for (i = 0; i < elf_h->e_shnum; i++) {
        char *a = get_shstr(elf_h, shstr_hdr, sect_hdr[i].sh_name);
        if (strcmp(a, name) == 0) return i;
    }
    return 0;
}

Elf32_Shdr *find_section(char *name, Elf32_Ehdr *elf_h) {
    int         index    = find_section_index(name, elf_h);
    Elf32_Shdr *sect_hdr = (Elf32_Shdr *)((uint32_t)elf_h + elf_h->e_shoff);
    return index == 0 ? (Elf32_Shdr *)NULL : &sect_hdr[index];
}

Elf32_Sym *get_symtab(Elf32_Ehdr *elf_h) {
    Elf32_Shdr *sect_hdr = (Elf32_Shdr *)((uint32_t)elf_h + elf_h->e_shoff);
    sect_hdr             = find_section(".symtab", elf_h);

    if (sect_hdr != NULL)
        return (Elf32_Sym *)((uint32_t)elf_h + (uint32_t)sect_hdr->sh_offset);
    else
        return NULL;
}

Elf32_Sym *find_symbol(char *name, Elf32_Ehdr *elf_h) {
    uint32_t i;
    char    *tname;

    Elf32_Shdr *symtab_sect = find_section(".symtab", elf_h);
    Elf32_Shdr *strtab_sect = find_section(".strtab", elf_h);

    logkf("Searching for symbol %s in elf @ 0x%08x\n", name, (size_t)elf_h);

    if (symtab_sect == NULL) {
        logkf("No has .symtab section\n");
        return NULL;
    }

    if (strtab_sect == NULL) {
        logkf("No has .strtab section\n");
        return NULL;
    }

    if (symtab_sect->sh_entsize != sizeof(Elf32_Sym)) {
        logkf("Wrong .symtab entry size\n");
        return NULL;
    }

    Elf32_Sym *syms = (Elf32_Sym *)((uint32_t)elf_h + symtab_sect->sh_offset);

    uint32_t n = symtab_sect->sh_size / symtab_sect->sh_entsize;
    logkf("Found %i entries in .symtab (sect at address 0x%x)\n", n, (unsigned int)syms);

    for (i = 0; i < n; i++) {
        tname = get_shstr(elf_h, strtab_sect, syms[i].st_name);
        if (strcmp(tname, name) == 0) { return &syms[i]; }
    }
    return NULL;
}