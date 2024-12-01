#pragma once

#include "ctypes.h"

typedef struct slist_sp *slist_sp_t;

struct slist_sp {
    char      *key;
    void      *val;
    slist_sp_t next;
};

slist_sp_t slist_sp_alloc(const char *key, void *val) __THROW;
void slist_sp_free(slist_sp_t list) __THROW;
void slist_sp_free_with(slist_sp_t list, void (*free_value)(void *)) __THROW;
slist_sp_t slist_sp_append(slist_sp_t list, const char *key, void *val) __THROW;
slist_sp_t slist_sp_prepend(slist_sp_t list, const char *key, void *val) __THROW;
void *slist_sp_get(slist_sp_t list, const char *key) __THROW;
slist_sp_t slist_sp_get_node(slist_sp_t list, const char *key) __THROW;
bool slist_sp_search(slist_sp_t list, void *val, const char **key) __THROW;
slist_sp_t slist_sp_search_node(slist_sp_t list, void *val) __THROW;
slist_sp_t slist_sp_delete(slist_sp_t list, const char *key) __THROW;
slist_sp_t slist_sp_delete_with(slist_sp_t list, const char *key, void (*free_value)(void *))__THROW;
slist_sp_t slist_sp_delete_node(slist_sp_t list, slist_sp_t node) __THROW;
slist_sp_t slist_sp_delete_node_with(slist_sp_t list, slist_sp_t node,void (*free_value)(void *)) __THROW;
size_t slist_sp_length(slist_sp_t list) __THROW;
void slist_sp_print(slist_sp_t list) __THROW;

#define slist_sp_foreach(list, node) for (slist_sp_t node = (list); node; node = node->next)
