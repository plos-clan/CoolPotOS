#pragma once

#include "elf.h"
#include "elf_parse.h"
#include "page.h"

Elf32_Half  find_section_index(char *name, Elf32_Ehdr *elf_h);
Elf32_Shdr *find_section(char *name, Elf32_Ehdr *elf_h);
Elf32_Sym  *get_symtab(Elf32_Ehdr *elf_h);
Elf32_Sym  *find_symbol(char *name, Elf32_Ehdr *elf_h);
bool        check_elf_magic(Elf32_Ehdr *hdr);
uint32_t    load_elf(Elf32_Ehdr *hdr, page_directory_t *dir);
void        load_segment(Elf32_Phdr *phdr, page_directory_t *dir, void *elf);
uint32_t    elf_load(size_t elf_size, uint8_t *elf_data);