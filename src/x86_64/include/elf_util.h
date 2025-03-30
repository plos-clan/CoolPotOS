#pragma once

#include "elf.h"
#include "module.h"
#include "page.h"

typedef void (*elf_start)(void);

elf_start load_executor_elf(cp_module_t *file, page_directory_t *dir);
bool      mmap_phdr_segment(Elf64_Ehdr *ehdr, Elf64_Phdr *phdrs, page_directory_t *directory,
                            bool is_user);
bool      elf_test_head(Elf64_Ehdr *ehdr);
void      load_segment(Elf64_Phdr *phdr, void *elf, page_directory_t *directory, bool is_user);
