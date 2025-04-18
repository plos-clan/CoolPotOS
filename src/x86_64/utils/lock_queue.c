#include "lock_queue.h"
#include "heap.h"
#include "krlibc.h"

lock_queue *queue_init() {
    lock_queue *q = malloc(sizeof(lock_queue));
    not_null_assets(q, "lock queue null");
    memset(q, 0, sizeof(lock_queue));
    q->head = q->tail = NULL;
    q->next_index     = 0;
    q->size           = 0;
    ticket_init(&q->lock);
    ticket_init(&q->iter_lock);
    return q;
}

size_t lock_queue_enqueue(lock_queue *q, void *data) {
    if (q == NULL) return -1;
    ticket_lock(&q->lock);
    lock_node *new_node = (lock_node *)malloc(sizeof(lock_node));
    if (!new_node) return -1;
    new_node->data  = data;
    new_node->next  = NULL;
    new_node->index = q->next_index++;

    if (q->head == NULL) {
        q->head = new_node;
    } else {
        lock_node *current = q->head;
        infinite_loop {
            if (current->next == NULL) {
                current->next = new_node;
                break;
            }
            current = current->next;
        }
    }
    q->tail = new_node;
    q->size++;

    return new_node->index;
}

size_t queue_enqueue(lock_queue *q, void *data) {
    if (q == NULL) return -1;
    ticket_lock(&q->lock);

    lock_node *new_node = (lock_node *)malloc(sizeof(lock_node));
    if (!new_node) return -1;
    new_node->data  = data;
    new_node->next  = NULL;
    new_node->index = q->next_index++;

    if (q->head == NULL) {
        q->head = new_node;
    } else {
        lock_node *current = q->head;
        infinite_loop {
            if (current->next == NULL) {
                current->next = new_node;
                break;
            }
            current = current->next;
        }
    }
    q->tail = new_node;
    q->size++;
    ticket_unlock(&q->lock);

    return new_node->index;
}

void *queue_remove_at(lock_queue *q, size_t index) {
    if (q == NULL) return NULL;
    ticket_lock(&q->lock);
    lock_node *current  = q->head;
    lock_node *previous = NULL;

    while (current != NULL) {
        if (current->index == index) {
            if (previous == NULL) {
                lock_node *new_head = current->next;
                q->head             = new_head;
            } else {
                previous->next = current->next;
            }
            free(current);
            q->size--;
            ticket_unlock(&q->lock);
            return NULL;
        }
        previous = current;
        current  = current->next;
    }
    ticket_unlock(&q->lock);
    return NULL;
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
