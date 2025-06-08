#pragma once

#include "elf.h"
#include "module.h"
#include "page.h"

typedef void (*elf_start)(void);

/**
 * 加载一个ELF可执行文件
 * @param file ELF文件句柄
 * @param dir 需要映射到的页表
 * @param offset 映射偏移
 * @param load_start 加载起始地址
 * @return == NULL ? 无法识别或正确加载ELF : ELF入口函数
 */
elf_start load_executor_elf(cp_module_t *file, page_directory_t *dir, uint64_t offset,
                            uint64_t *load_start);

/**
 * 映射ELF文件的程序头段
 * @param ehdr ELF文件头
 * @param phdrs PHDR段
 * @param directory 页表项
 * @param is_user ? 用户态映射 : 内核态映射
 * @param offset 映射偏移
 * @param load_start 加载起始地址
 * @return 映射是否成功
 */
bool mmap_phdr_segment(Elf64_Ehdr *ehdr, Elf64_Phdr *phdrs, page_directory_t *directory,
                       bool is_user, uint64_t offset, uint64_t *load_start);

/**
 * 校验ELF文件头
 * @param ehdr 文件头
 * @return 是否是ELF文件
 */
bool elf_test_head(Elf64_Ehdr *ehdr);

/**
 * 加载一个ELF段
 * @param phdr 程序头段
 * @param elf ELF文件数据
 * @param directory 页表
 * @param is_user ? 用户态 : 内核态
 * @param offset 映射偏移
 * @param load_start 加载起始地址
 */
void load_segment(Elf64_Phdr *phdr, void *elf, page_directory_t *directory, bool is_user,
                  uint64_t offset, uint64_t *load_start);

/**
 * 判断 elf 是否是动态链接的
 * @param ehdr
 * @return 是否为动态 (没有程序头也会返回 false)
 */
bool is_dynamic(Elf64_Ehdr *ehdr);
