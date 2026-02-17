#pragma once

#include "lock.h"
#include "types.h"

typedef void (*data_free_func_t)(void *data, void *arg);

typedef struct list_node {
    struct list_node *prev;
    struct list_node *next;
    void *data;
} list_node_t;

typedef struct list_queue {
    list_node_t *head;
    list_node_t *tail;
    size_t size;
    spin_t lock;
} list_queue_t;

#define qlist_foreach(list, node) for (list_node_t *node = (list)->head; node; node = node->next)

list_queue_t *copy_list_queue(
    list_queue_t *src_queue, void *(*copy)(void *),
    void (*index_clone)(void *, list_node_t *index));
list_node_t *list_enqueue(list_queue_t *queue, void *data);
void list_remove_node(list_queue_t *queue, list_node_t *node_to_remove);
void free_llist_queue(list_queue_t *queue, data_free_func_t data_free_func, void *arg);
list_queue_t *create_llist_queue();
