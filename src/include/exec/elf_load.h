#pragma once

#include "exec/elf.h"
#include "mem/page.h"
#include "task/task.h"

bool arch_elf_test_head(Elf64_Ehdr *ehdr);
bool is_dynamic(Elf64_Ehdr *ehdr);
bool mmap_phdr_segment(
    Elf64_Ehdr *ehdr, Elf64_Phdr *phdrs, page_directory_t *directory, bool is_user, uint64_t offset,
    uint64_t *load_start, uint64_t *load_size);
void load_segment(
    Elf64_Phdr *phdr, void *elf, page_directory_t *directory, bool is_user, uint64_t offset,
    uint64_t *load_start);
void *load_executor_elf(
    uint8_t *data, page_directory_t *dir, uint64_t offset, uint64_t *load_start, pcb_t process);
void *load_interpreter_elf(
    uint8_t *data, page_directory_t *dir, uint64_t *load_start, uint8_t **link_data,
    size_t *link_size);
void launch_init_process();
