#include "lock_queue.h"
#include "heap.h"
#include "krlibc.h"
#include "lock.h"

lock_queue *queue_init() {
    lock_queue *q = malloc(sizeof(lock_queue));
    not_null_assets(q, "lock queue null");
    memset(q, 0, sizeof(lock_queue));
    q->head = q->tail = NULL;
    q->next_index     = 0;
    q->size           = 0;
    q->lock           = SPIN_INIT;
    q->iter_lock      = SPIN_INIT;
    return q;
}

size_t lock_queue_enqueue(lock_queue *q, void *data) {
    if (q == NULL) return -1;
    spin_lock(q->lock);
    lock_node *new_node = (lock_node *)malloc(sizeof(lock_node));
    if (!new_node) return -1;
    new_node->data  = data;
    new_node->next  = NULL;
    new_node->index = q->next_index++;

    if (q->head == NULL) {
        q->head = new_node;
    } else {
        lock_node *current = q->head;
        loop {
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
    spin_lock(q->lock);

    lock_node *new_node = (lock_node *)malloc(sizeof(lock_node));
    if (!new_node) return -1;
    new_node->data  = data;
    new_node->next  = NULL;
    new_node->index = q->next_index++;

    if (q->head == NULL) {
        q->head = new_node;
    } else {
        lock_node *current = q->head;
        loop {
            if (current->next == NULL) {
                current->next = new_node;
                break;
            }
            current = current->next;
        }
    }
    q->tail = new_node;
    q->size++;
    spin_unlock(q->lock);

    return new_node->index;
}

size_t queue_enqueue_id(lock_queue *q, void *data, size_t id) {
    if (q == NULL) return -1;
    spin_lock(q->lock);

    lock_node *new_node = (lock_node *)malloc(sizeof(lock_node));
    if (!new_node) return -1;
    new_node->data  = data;
    new_node->next  = NULL;
    new_node->index = id;

    if (q->head == NULL) {
        q->head = new_node;
    } else {
        lock_node *current = q->head;
        loop {
            if (current->next == NULL) {
                current->next = new_node;
                break;
            }
            current = current->next;
        }
    }
    q->tail = new_node;
    q->size++;
    spin_unlock(q->lock);

    return new_node->index;
}

void *queue_get(lock_queue *q, size_t index) {
    if (q == NULL) return NULL;
    spin_lock(q->lock);
    lock_node *current = q->head;

    while (current != NULL) {
        if (current->index == index) {
            spin_unlock(q->lock);
            return current->data;
        }
        current = current->next;
    }
    spin_unlock(q->lock);
    return NULL;
}

void *queue_remove_at(lock_queue *q, size_t index) {
    if (q == NULL) return NULL;
    spin_lock(q->lock);
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
            void *handle = current->data;
            free(current);
            q->size--;
            spin_unlock(q->lock);
            return handle;
        }
        previous = current;
        current  = current->next;
    }
    spin_unlock(q->lock);
    return NULL;
}

void *queue_dequeue(lock_queue *q) {
    spin_lock(q->lock);
    if (!q->head) {
        spin_unlock(q->lock);
        return NULL;
    }
    lock_node *temp = q->head;
    void      *data = temp->data;
    q->head         = q->head->next;
    if (!q->head) { q->tail = NULL; }
    q->size--;
    spin_unlock(q->lock);
    free(temp);
    return data;
}

lock_queue *queue_copy(lock_queue *src, void *(*data_copy)(void *)) {
    if (!src) return NULL;

    spin_lock(src->lock);

    lock_queue *new_q = queue_init();

    lock_node *current = src->head;
    while (current != NULL) {
        lock_node *new_node = malloc(sizeof(lock_node));
        if (!new_node) {
            spin_unlock(src->lock);
            return NULL;
        }
        new_node->data  = data_copy != NULL ? data_copy(current->data) : current->data;
        new_node->index = current->index;
        new_node->next  = NULL;

        if (!new_q->head) {
            new_q->head = new_q->tail = new_node;
        } else {
            new_q->tail->next = new_node;
            new_q->tail       = new_node;
        }

        current = current->next;
    }

    new_q->size       = src->size;
    new_q->next_index = src->next_index;

    spin_unlock(src->lock);
    return new_q;
}

void queue_iterate(lock_queue *q, void (*callback)(void *, void *), void *argument) {
    spin_trylock(q->lock);
    lock_node *current = q->head;
    while (current) {
        callback(current->data, argument);
        current = current->next;
    }
    spin_unlock(q->lock);
}

void queue_destroy(lock_queue *q) {
    spin_trylock(q->lock);
    lock_node *current = q->head;
    while (current) {
        lock_node *next = current->next;
        free(current);
        current = next;
    }
    q->head = q->tail = NULL;
    spin_unlock(q->lock);
    spin_unlock(q->iter_lock);
    free(q);
}
