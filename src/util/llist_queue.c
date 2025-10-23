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

void free_llist_queue(list_queue_t *queue, data_free_func_t data_free_func, void *arg) {
    if (!queue) return;
    spin_lock(queue->lock);

    list_node_t *current = queue->head;
    list_node_t *next_node;

    while (current != NULL) {
        next_node = current->next;
        if (data_free_func && current->data) {
            data_free_func(current->data,arg);
        }
        free(current);
        current = next_node;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    spin_unlock(queue->lock);
    free(queue);
}

list_queue_t *copy_list_queue(list_queue_t *src_queue, void*(*copy)(void*)) {
    if (!src_queue || !copy) return NULL;
    spin_lock(src_queue->lock);
    list_queue_t *new_queue = create_llist_queue();
    if (!new_queue) {
        spin_unlock(src_queue->lock);
        return NULL;
    }
    list_node_t *current_src = src_queue->head;
    list_node_t *prev_new = NULL;

    while (current_src != NULL) {
        list_node_t *new_node = (list_node_t *)malloc(sizeof(list_node_t));
        if (!new_node) {
            list_node_t *cleanup_node = new_queue->head;
            while(cleanup_node != NULL) {
                list_node_t *next = cleanup_node->next;
                free(cleanup_node);
                cleanup_node = next;
            }
            free(new_queue);

            spin_unlock(src_queue->lock);
            return NULL;
        }

        // 浅拷贝数据指针
        new_node->data = copy(current_src->data);

        new_node->prev = prev_new;
        new_node->next = NULL;

        if (new_queue->size == 0 || prev_new == NULL) {
            new_queue->head = new_node;
        } else {
            prev_new->next = new_node;
        }

        new_queue->tail = new_node;
        prev_new = new_node;
        new_queue->size++;

        current_src = current_src->next;
    }

    spin_unlock(src_queue->lock);
    return new_queue;
}
