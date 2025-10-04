#include "map.h"
#include "heap.h"

static size_t hash_ptr(void *ptr) {
    uintptr_t p = (uintptr_t)ptr;
    return (p >> 4) * 2654435761u;
}

static void map_resize(map *map0) {
    size_t      new_capacity = map0->capacity * 2;
    map_entry **new_buckets  = (map_entry **)calloc(new_capacity, sizeof(map_entry *));
    if (!new_buckets) return;

    for (size_t i = 0; i < map0->capacity; ++i) {
        map_entry *entry = map0->buckets[i];
        while (entry) {
            map_entry *next      = entry->next;
            size_t     new_index = hash_ptr(entry->key) % new_capacity;

            entry->next            = new_buckets[new_index];
            new_buckets[new_index] = entry;

            entry = next;
        }
    }

    free(map0->buckets);
    map0->buckets  = new_buckets;
    map0->capacity = new_capacity;
}

map *map_create(size_t capacity) {
    map *map0 = (map *)malloc(sizeof(map));
    if (!map0) return NULL;
    map0->capacity = capacity;
    map0->buckets  = (map_entry **)calloc(capacity, sizeof(map_entry *));
    if (!map0->buckets) {
        free(map0);
        return NULL;
    }
    map0->lock = SPIN_INIT;
    map0->size = 0;
    return map0;
}

void map_set(map *map0, void *key, void *value) {
    spin_lock(map0->lock);
    size_t index = hash_ptr(key) % map0->capacity;
    if ((double)map0->size / map0->capacity > LOAD_FACTOR_THRESHOLD) { map_resize(map0); }

    map_entry *entry = map0->buckets[index];
    while (entry) {
        if (entry->key == key) {
            entry->value = value;
            spin_unlock(map0->lock);
            return;
        }
        entry = entry->next;
    }

    map_entry *new_entry = (map_entry *)malloc(sizeof(map_entry));
    new_entry->key       = key;
    new_entry->value     = value;
    new_entry->next      = map0->buckets[index];
    map0->buckets[index] = new_entry;

    spin_unlock(map0->lock);
}

void map_remove(map *map0, void *key) {
    spin_lock(map0->lock);
    size_t     index = hash_ptr(key) % map0->capacity;
    map_entry *entry = map0->buckets[index];
    map_entry *prev  = NULL;
    while (entry) {
        if (entry->key == key) {
            if (prev)
                prev->next = entry->next;
            else
                map0->buckets[index] = entry->next;

            free(entry);
            spin_unlock(map0->lock);
            return;
        }
        prev  = entry;
        entry = entry->next;
    }
    map0->size--;
    spin_unlock(map0->lock);
}

void *map_get(map *map0, void *key) {
    size_t index = hash_ptr(key) % map0->capacity;

    spin_lock(map0->lock);

    map_entry *entry = map0->buckets[index];
    while (entry) {
        if (entry->key == key) {
            void *val = entry->value;
            spin_unlock(map0->lock);
            return val;
        }
        entry = entry->next;
    }

    spin_unlock(map0->lock);
    return NULL;
}

void map_destroy(map *map0) {
    spin_lock(map0->lock);
    for (size_t i = 0; i < map0->capacity; ++i) {
        map_entry *entry = map0->buckets[i];
        while (entry) {
            map_entry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(map0->buckets);
    spin_unlock(map0->lock);
    free(map0);
}
