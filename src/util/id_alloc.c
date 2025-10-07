#include "id_alloc.h"
#include "heap.h"

id_allocator_t *id_allocator_create(uint32_t max_ids) {
    id_allocator_t *alloc = malloc(sizeof(id_allocator_t));
    if (!alloc) return NULL;

    uint32_t bitmap_size = (max_ids + BITS_PER_WORD - 1) / BITS_PER_WORD;
    alloc->bitmap        = calloc(bitmap_size, sizeof(uint32_t));
    if (!alloc->bitmap) {
        free(alloc);
        return NULL;
    }

    alloc->max_ids    = max_ids;
    alloc->free_count = max_ids;
    alloc->next_id    = 0;
    return alloc;
}

int32_t id_alloc(id_allocator_t *allocator) {
    if (!allocator || allocator->free_count == 0) return -1;

    uint32_t start = allocator->next_id;
    uint32_t id    = start;

    do {
        uint32_t word_index = id / BITS_PER_WORD;
        uint32_t bit_index  = id % BITS_PER_WORD;
        uint32_t mask       = 1U << bit_index;

        if ((allocator->bitmap[word_index] & mask) == 0) {
            allocator->bitmap[word_index] |= mask;
            allocator->free_count--;
            allocator->next_id = (id + 1) % allocator->max_ids;
            return id;
        }

        id = (id + 1) % allocator->max_ids;
    } while (id != start);

    return -1;
}

bool id_free(id_allocator_t *allocator, uint32_t id) {
    if (!allocator || id >= allocator->max_ids) return false;

    uint32_t word_index = id / BITS_PER_WORD;
    uint32_t bit_index  = id % BITS_PER_WORD;
    uint32_t mask       = 1U << bit_index;
    if (allocator->bitmap[word_index] & mask) {
        allocator->bitmap[word_index] &= ~mask;
        allocator->free_count++;
        return true;
    }
    return false;
}
