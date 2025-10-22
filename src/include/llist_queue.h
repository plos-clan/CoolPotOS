#pragma once

#include "types.h"
#include "lock.h"

typedef struct list_node {
    struct list_node *prev;
    struct list_node *next;
    void             *data;
} list_node_t;

typedef struct list_queue {
    list_node_t *head;
    list_node_t *tail;
    size_t       size;
    spin_t       lock;
} list_queue_t;

#define qlist_foreach(list_ptr, node) \
    for (list_node_t *node = (list_ptr)->head; node != NULL; node = node->next)

list_node_t *list_enqueue(list_queue_t *queue, void *data);
void list_remove_node(list_queue_t *queue, list_node_t *node_to_remove);
list_queue_t *create_llist_queue();
