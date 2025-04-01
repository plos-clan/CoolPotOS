#include "lock_queue.h"
#include "alloc.h"
#include "krlibc.h"

lock_queue *queue_init() {
    lock_queue *q = malloc(sizeof(lock_queue));
    not_null_assets(q);
    memset(q, 0, sizeof(lock_queue));
    q->head = q->tail = NULL;
    q->next_index     = 0;
    q->size           = 0;
    ticket_init(&q->lock);
    ticket_init(&q->iter_lock);
    return q;
}

int queue_enqueue(lock_queue *q, void *data) {
    if (q == NULL) return -1;
    lock_node *new_node = (lock_node *)malloc(sizeof(lock_node));
    if (!new_node) return -1;
    new_node->data  = data;
    new_node->next  = NULL;
    new_node->index = q->next_index++;

    ticket_lock(&q->lock);
    if (q->tail) {
        q->tail->next = new_node;
    } else {
        q->head = new_node;
    }
    q->tail = new_node;
    q->size++;
    ticket_unlock(&q->lock);

    return new_node->index;
}

void *queue_remove_at(lock_queue *q, size_t index) {
    if (index < 0 || q == NULL) return NULL;
    ticket_trylock(&q->lock);
    lock_node *current = q->head;
    lock_node *prev    = NULL;
    while (current && current->index != index) {
        prev    = current;
        current = current->next;
    }
    if (!current) {
        ticket_unlock(&q->lock);
        return NULL;
    }
    void *data = current->data;
    if (prev) {
        prev->next = current->next;
    } else {
        q->head = current->next;
    }
    if (!q->head) { q->tail = NULL; }
    q->size--;
    free(current);
    ticket_unlock(&q->lock);
    return data;
}

void *queue_dequeue(lock_queue *q) {
    ticket_trylock(&q->lock);
    if (!q->head) {
        ticket_unlock(&q->lock);
        return NULL;
    }
    lock_node *temp = q->head;
    void      *data = temp->data;
    q->head         = q->head->next;
    if (!q->head) { q->tail = NULL; }
    q->size--;
    ticket_unlock(&q->lock);
    free(temp);
    return data;
}

void queue_iterate(lock_queue *q, void (*callback)(void *, void *), void *argument) {
    ticket_trylock(&q->lock);
    lock_node *current = q->head;
    while (current) {
        callback(current->data, argument);
        current = current->next;
    }
    ticket_unlock(&q->lock);
}

void queue_destroy(lock_queue *q) {
    ticket_trylock(&q->lock);
    lock_node *current = q->head;
    while (current) {
        lock_node *next = current->next;
        free(current);
        current = next;
    }
    q->head = q->tail = NULL;
    ticket_unlock(&q->lock);
    ticket_unlock(&q->iter_lock);
    free(q);
}
