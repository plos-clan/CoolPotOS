#pragma once

#define BLOCK_SIZE 64

#include "krlibc.h"
#include "lock.h"


typedef struct {
    void *elements[BLOCK_SIZE];
    _Atomic size_t ref_count;
} cow_block;

typedef struct {
    cow_block **blocks;       // 块指针数组
    size_t block_count;       // blocks 数组中当前使用的块数量
    size_t blocks_capacity;   // blocks 数组的总容量
    _Atomic size_t size;              // 列表中元素的总数量 (原子性更新)
    spin_t lock;              // 保护所有写操作的自旋锁
} cow_arraylist;

#define cow_foreach(list, element) \
    for (size_t __cow_i = 0, __cow_size = cow_list_size(list); __cow_i < __cow_size; __cow_i++) \
        if (((element) = cow_list_get((list), __cow_i)) != NULL)

cow_arraylist *cow_list_create();
void cow_list_destroy(cow_arraylist *list);
void *cow_list_get(cow_arraylist *list, size_t index);
int cow_list_set(cow_arraylist *list, size_t index, void *element);
size_t cow_list_add(cow_arraylist *list, void *element);
void * cow_list_remove(cow_arraylist *list, size_t index);
size_t cow_list_size(cow_arraylist *list);