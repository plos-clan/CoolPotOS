#pragma once

#define BITS_PER_WORD (sizeof(uint32_t) * 8)

#include "cptype.h"

typedef struct {
    uint32_t *bitmap;     // 位图数组
    uint32_t  max_ids;    // 最大 ID 数量
    uint32_t  free_count; // 空闲 ID 计数
    uint32_t  next_id;    // 下一个尝试分配的 ID
} id_allocator_t;

id_allocator_t *id_allocator_create(uint32_t max_ids);
int32_t         id_alloc(id_allocator_t *);
bool            id_free(id_allocator_t *, uint32_t id);
