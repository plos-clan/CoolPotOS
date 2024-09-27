#include "../../../include/mpool.h"

int freelists_size2id(size_t size) {
    if (size < 64) return 0;
    if (size < 256) return 1;
    if (size >= FREELIST_MAXBLKSIZE) return -1;
    return (32 - 9) - __builtin_clz(size) + 2;
}

void freelist_put(freelist_t *list_p, freelist_t ptr) {
    ptr->next = *list_p;
    ptr->prev = NULL;
    *list_p   = ptr;
    if (ptr->next) ptr->next->prev = ptr;
}

freelist_t freelist_detach(freelist_t list, freelist_t ptr) {
    if (list == ptr) return ptr->next;
    if (ptr->next) ptr->next->prev = ptr->prev;
    if (ptr->prev) ptr->prev->next = ptr->next;
    return list;
}

void *freelists_detach(freelists_t lists, int id, freelist_t ptr) {
    if (lists[id] == ptr) {
        lists[id] = ptr->next;
        return ptr;
    }

    if (ptr->next) ptr->next->prev = ptr->prev;
    if (ptr->prev) ptr->prev->next = ptr->next;
    return ptr;
}

void *freelist_match(freelist_t *list_p, size_t size) {
    for (freelist_t list = *list_p; list != NULL; list = list->next) {
        size_t tgt_size = blk_size(list);
        if (tgt_size >= size) {
            *list_p = freelist_detach(*list_p, list);
            return list;
        }
    }
    return NULL;
}

void *freelists_match(freelists_t lists, size_t size) {
    int id = freelists_size2id(size);
    if (id < 0) return NULL;
    for (; id < FREELIST_NUM; id++) {
        for (freelist_t list = lists[id]; list != NULL; list = list->next) {
            size_t tgt_size = blk_size(list);
            if (tgt_size >= size) return freelists_detach(lists, id, list);
        }
    }
    return NULL;
}

bool freelists_put(freelists_t lists, void *_ptr) {
    freelist_t ptr  = _ptr;
    size_t     size = blk_size(ptr);
    int        id   = freelists_size2id(size);
    if (id < 0) return false;
    ptr->next = lists[id];
    ptr->prev = NULL;
    lists[id] = ptr;
    if (ptr->next) ptr->next->prev = ptr;
    return true;
}