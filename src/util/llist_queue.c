#include "llist_queue.h"
#include "mem/heap.h"

list_queue_t *create_llist_queue(){
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

    if (queue->size == 0) {
        // 队列为空
        new_node->prev = NULL;
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        // 队列非空，插入到尾部
        new_node->prev = queue->tail;
        queue->tail->next = new_node;
        queue->tail = new_node;
    }

    queue->size++;
    spin_unlock(queue->lock);
    return new_node;
}

void list_remove_node(list_queue_t *queue, list_node_t *node_to_remove) {
    if (!queue || !node_to_remove) return;

    if (node_to_remove->prev) {
        node_to_remove->prev->next = node_to_remove->next;
    } else {
        // 被删除的是头部节点
        queue->head = node_to_remove->next;
    }

    if (node_to_remove->next) {
        node_to_remove->next->prev = node_to_remove->prev;
    } else {
        // 被删除的是尾部节点
        queue->tail = node_to_remove->prev;
    }

    free(node_to_remove);
    queue->size--;
}
