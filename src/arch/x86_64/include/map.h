#pragma once

#define LOAD_FACTOR_THRESHOLD 0.75

#include "ctype.h"
#include "lock.h"

typedef struct map_entry {
    void             *key;
    void             *value;
    struct map_entry *next;
} map_entry;

typedef struct map {
    spin_t      lock;
    size_t      capacity;
    size_t      size;
    map_entry **buckets;
} map;

map  *map_create(size_t capacity);
void  map_set(map *map0, void *key, void *value);
void  map_remove(map *map0, void *key);
void  map_destroy(map *map0);
void *map_get(map *map0, void *key);
