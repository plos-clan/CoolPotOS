#pragma once

// VMA标志定义
#define VMA_READ   0x1
#define VMA_WRITE  0x2
#define VMA_EXEC   0x4
#define VMA_SHARED 0x8
#define VMA_ANON   0x10
#define VMA_SHM    0x20

#include "ctype.h"

// VMA类型
typedef enum {
    VMA_TYPE_ANON, // 匿名映射
    VMA_TYPE_FILE, // 文件映射
    VMA_TYPE_SHM   // 共享内存
} vma_type_t;

// VMA结构体
typedef struct vma {
    unsigned long vm_start;  // 起始地址
    unsigned long vm_end;    // 结束地址
    unsigned long vm_flags;  // 权限标志
    vma_type_t    vm_type;   // VMA类型
    int           vm_fd;     // 文件描述符（文件映射用）
    int64_t       vm_offset; // 文件偏移量
    int           shm_id;    // 共享内存ID
    char         *vm_name;   // VMA 名称
    struct vma   *vm_next;   // 链表下一个节点
    struct vma   *vm_prev;   // 链表前一个节点
} vma_t;

// VMA管理器
typedef struct {
    vma_t        *vma_list; // VMA链表头
    unsigned long vm_total; // 总虚拟内存大小
    unsigned long vm_used;  // 已使用虚拟内存
} vma_manager_t;

vma_t *vma_alloc(void);
void   vma_free(vma_t *vma);
vma_t *vma_find(vma_manager_t *mgr, unsigned long addr);
vma_t *vma_find_intersection(vma_manager_t *mgr, unsigned long start, unsigned long end);
int    vma_insert(vma_manager_t *mgr, vma_t *vma);
int    vma_remove(vma_manager_t *mgr, vma_t *vma);
int    vma_split(vma_t *vma, unsigned long addr);
int    vma_merge(vma_t *vma1, vma_t *vma2);
int    vma_unmap_range(vma_manager_t *mgr, uintptr_t start, uintptr_t end);
void   vma_manager_exit_cleanup(vma_manager_t *mgr);
