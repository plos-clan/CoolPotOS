#include "cow_arraylist.h"
#include "mem/heap.h"

static size_t calculate_block_count(size_t total_size) {
    if (total_size == 0) return 0;
    return (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

static void block_retain(cow_block *block) {
    if (block) {
        __atomic_add_fetch((size_t*)&block->ref_count, 1, __ATOMIC_RELAXED);
    }
}

static void block_release(cow_block *block) {
    if (block) {
        _Atomic size_t new_count = __atomic_sub_fetch((size_t*)&block->ref_count, 1, __ATOMIC_ACQ_REL);
        if (new_count == 0) {
            free(block);
        }
    }
}

cow_arraylist *cow_list_create() {
    cow_arraylist *list = (cow_arraylist *)malloc(sizeof(cow_arraylist));
    if (list == NULL) return NULL;
    list->size = 0;
    list->block_count = 0;
    list->blocks_capacity = 4;
    list->lock = SPIN_INIT;
    list->blocks = (cow_block **)calloc(list->blocks_capacity, sizeof(cow_block *));
    if ((void*)list->blocks == NULL) {
        free(list);
        return NULL;
    }

    return list;
}

void cow_list_destroy(cow_arraylist *list) {
    if (list == NULL) return;
    for (size_t i = 0; i < list->block_count; i++) {
        block_release(list->blocks[i]);
    }
    free((void*)list->blocks);
    free(list);
}

size_t cow_list_size(cow_arraylist *list) {
    return __atomic_load_n((size_t*)&list->size, __ATOMIC_ACQUIRE);
}

void *cow_list_get(cow_arraylist *list, size_t index) {
    size_t current_size = list->size;
    if (index >= current_size) {
        return NULL;
    }

    size_t block_index = index / BLOCK_SIZE;
    size_t element_index = index % BLOCK_SIZE;

    cow_block *block = list->blocks[block_index];

    return block->elements[element_index];
}

int cow_list_set(cow_arraylist *list, size_t index, void *element) {
    if (index >= list->size) {
        return -1;
    }

    size_t block_index = index / BLOCK_SIZE;
    size_t element_index = index % BLOCK_SIZE;
    spin_lock(list->lock);

    cow_block *old_block = list->blocks[block_index];
    cow_block *new_block = (cow_block *)malloc(sizeof(cow_block));
    if (new_block == NULL) {
        spin_unlock(list->lock);
        return -1;
    }

    memcpy(new_block, old_block, sizeof(cow_block));
    __atomic_store_n((size_t *)&new_block->ref_count, 1, __ATOMIC_RELEASE);
    new_block->elements[element_index] = element;
    list->blocks[block_index] = new_block;
    block_release(old_block);
    spin_unlock(list->lock);
    return 0;
}

size_t cow_list_add(cow_arraylist *list, void *element) {
    size_t new_size = 0;

    spin_lock(list->lock);

    size_t old_size = list->size;
    new_size = old_size + 1;
    size_t old_block_count = list->block_count;
    size_t new_block_count = calculate_block_count(new_size);
    // 数组扩容措施
    if (new_block_count > list->blocks_capacity) {
        size_t new_capacity = list->blocks_capacity * 2;
        cow_block **new_blocks_array = (cow_block **)realloc((void*)list->blocks, new_capacity * sizeof(cow_block*));

        if ((void*)new_blocks_array == NULL) {
            spin_unlock(list->lock);
            return 0;
        }

        list->blocks = new_blocks_array;
        list->blocks_capacity = new_capacity;
    }

    size_t target_block_index = new_block_count - 1;
    size_t target_element_index = old_size % BLOCK_SIZE;

    cow_block *target_block = list->blocks[target_block_index];
    cow_block *old_block_to_release = NULL;

    // 处理块复制或新块分配
    if (new_block_count > old_block_count) {
        // 需要创建新的块 (首次分配)
        target_block = (cow_block *)calloc(1, sizeof(cow_block));
        if (target_block == NULL) {
            spin_unlock(list->lock);
            return 0;
        }

        // 设置初始引用计数为 1 (修正：显式转换为 (size_t *))
        __atomic_store_n((size_t *)&target_block->ref_count, 1, __ATOMIC_RELEASE);

        list->blocks[old_block_count] = target_block;
        list->block_count = new_block_count;
    } else {
        // 在旧块中追加，需要 COW 复制
        old_block_to_release = target_block;

        cow_block *new_block = (cow_block *)malloc(sizeof(cow_block));
        if (new_block == NULL) {
            spin_unlock(list->lock);
            return 0;
        }

        memcpy(new_block, old_block_to_release, sizeof(cow_block));
        // 设置新块的初始引用计数为 1 (修正：显式转换为 (size_t *))
        __atomic_store_n((size_t *)&new_block->ref_count, 1, __ATOMIC_RELEASE);
        target_block = new_block;

        // 替换块指针
        list->blocks[target_block_index] = new_block;

        // 释放旧块的引用
        block_release(old_block_to_release);
    }

    target_block->elements[target_element_index] = element;
    __atomic_store_n((size_t *)&list->size, new_size, __ATOMIC_RELEASE);
    spin_unlock(list->lock);

    return new_size;
}

void * cow_list_remove(cow_arraylist *list, size_t index) {
    if (index >= list->size || list->size == 0) {
        return NULL;
    }

    void * removed_element = NULL;

    spin_lock(list->lock);

    size_t old_size = list->size;
    size_t final_block_count = calculate_block_count(old_size - 1);

    // 保存要移除的元素
    size_t block_idx_rem = index / BLOCK_SIZE;
    size_t element_idx_rem = index % BLOCK_SIZE;
    removed_element = list->blocks[block_idx_rem]->elements[element_idx_rem];

    // 迭代并进行块级 COW 复制和数据移动 (O(N/B) 次块复制)
    for (size_t i = index; i < old_size - 1; i++) {
        size_t src_block_idx = (i + 1) / BLOCK_SIZE;
        size_t src_element_idx = (i + 1) % BLOCK_SIZE;
        size_t dest_block_idx = i / BLOCK_SIZE;
        size_t dest_element_idx = i % BLOCK_SIZE;

        // 只有当目标块变化时才需要 COW 复制 (或者在起点)
        if (i == index || dest_element_idx == 0) {
            cow_block *dest_block_old = list->blocks[dest_block_idx];

            cow_block *dest_block_new = (cow_block *)malloc(sizeof(cow_block));
            if (dest_block_new == NULL) {
                spin_unlock(list->lock);
                return removed_element;
            }

            memcpy(dest_block_new, dest_block_old, sizeof(cow_block));
            // 设置新块的初始引用计数为 1 (修正：显式转换为 (size_t *))
            __atomic_store_n((size_t *)&dest_block_new->ref_count, 1, __ATOMIC_RELEASE);

            // 替换块
            list->blocks[dest_block_idx] = dest_block_new;
            // 释放旧块引用
            block_release(dest_block_old);
        }

        // 移动数据
        void * source_element = list->blocks[src_block_idx]->elements[src_element_idx];
        list->blocks[dest_block_idx]->elements[dest_element_idx] = source_element;
    }

    // 处理最后一个元素所在的块：清空最后一个元素并处理块释放
    size_t last_element_pos = old_size - 1;
    size_t last_block_idx = last_element_pos / BLOCK_SIZE;
    size_t last_element_idx = last_element_pos % BLOCK_SIZE;

    if (final_block_count < list->block_count) {
        // 块被清空，需要释放整个块
        cow_block *last_block = list->blocks[list->block_count - 1];
        list->blocks[list->block_count - 1] = NULL;
        list->block_count--;

        // 释放旧块引用
        block_release(last_block);
    } else {
        // 块未清空，需 COW 复制最后一个块并清除最后一个元素
        cow_block *target_block_old = list->blocks[last_block_idx];
        cow_block *target_block_new = (cow_block *)malloc(sizeof(cow_block));

        if (target_block_new != NULL) {
            memcpy(target_block_new, target_block_old, sizeof(cow_block));
            // 修正：显式转换为 (size_t *)
            __atomic_store_n((size_t *)&target_block_new->ref_count, 1, __ATOMIC_RELEASE);
            target_block_new->elements[last_element_idx] = NULL; // 清除最后一个元素

            list->blocks[last_block_idx] = target_block_new;
            // 释放旧块引用
            block_release(target_block_old);
        }
    }

    __atomic_store_n((size_t *)&list->size, old_size - 1, __ATOMIC_RELEASE);
    spin_unlock(list->lock);

    return removed_element;
}
