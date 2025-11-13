#include "llist_queue.h"
#include "mem/heap.h"

list_queue_t *create_llist_queue() {
    return calloc(1, sizeof(list_queue_t));
}

list_node_t *list_enqueue(list_queue_t *queue, void *data) {
    if (!queue) return NULL;
    spin_lock(queue->lock);
    list_node_t *new_node = (list_node_t *)malloc(sizeof(list_node_t));
    if (!new_node) {
        spin_unlock(queue->lock);
        return NULL;
    }

    new_node->data = data;
    new_node->next = NULL;

    if (queue->head == NULL) {
        queue->head = new_node;
    } else {
        list_node_t *current = queue->head;
        while (true) {
            if (current->next == NULL) {
                current->next = new_node;
                break;
            }
            current = current->next;
        }
    }
    queue->tail = new_node;

    queue->size++;
    spin_unlock(queue->lock);
    return new_node;
}

void list_remove_node(list_queue_t *queue, list_node_t *node_to_remove) {
    if (!queue || !node_to_remove) return;

    spin_lock(queue->lock); // 加锁保护链表结构
    list_node_t *current  = queue->head;
    list_node_t *previous = NULL;

    while (current != NULL) {
        if (current == node_to_remove) {
            if (previous == NULL) {
                list_node_t *new_head = current->next;
                queue->head           = new_head;
            } else {
                previous->next = current->next;
            }
            void *handle = current->data;
            free(current);
            queue->size--;
            spin_unlock(queue->lock);
            return;
        }
        previous = current;
        current  = current->next;
    }
    spin_unlock(queue->lock);
}

void free_llist_queue(list_queue_t *queue, data_free_func_t data_free_func, void *arg) {
    if (!queue) return;
    spin_lock(queue->lock);

    list_node_t *current = queue->head;
    list_node_t *next_node;

    while (current != NULL) {
        next_node = current->next;
        if (data_free_func && current->data) { data_free_func(current->data, arg); }
        free(current);
        current = next_node;
    }
    queue->head = NULL;
    queue->size = 0;
    spin_unlock(queue->lock);
    free(queue);
}

list_queue_t *copy_list_queue(list_queue_t *src_queue, void *(*copy)(void *),
                              void (*index_clone)(void *, list_node_t *index)) {
    if (!src_queue || !copy) return NULL;
    spin_lock(src_queue->lock);

    list_queue_t *new_queue = create_llist_queue();
    if (!new_queue) {
        spin_unlock(src_queue->lock);
        return NULL;
    }

    for (list_node_t *cur = src_queue->head; cur; cur = cur->next) {
        void *copied = copy(cur->data);
        if (!copied) {
            free_llist_queue(new_queue, NULL, NULL);
            spin_unlock(src_queue->lock);
            return NULL;
        }
        list_node_t *node = list_enqueue(new_queue, copied);
        if (node == NULL) {
            free(copied);
            free_llist_queue(new_queue, NULL, NULL);
            spin_unlock(src_queue->lock);
            return NULL;
        }
        if(index_clone) index_clone(copied,node);
    }

    spin_unlock(src_queue->lock);
    return new_queue;
}
