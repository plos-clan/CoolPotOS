#include "atom_queue.h"
#include "heap.h"
#include "io.h"
#include "krlibc.h"

atom_queue *create_atom_queue(uint64_t size) {
    if (size == 0 || (size & (size - 1)) != 0) { return NULL; }
    atom_queue *queue = (atom_queue *)malloc(sizeof(atom_queue));
    memset(queue, 0, sizeof(atom_queue));
    queue->buf  = (uint8_t *)malloc(size * sizeof(uint8_t));
    queue->mask = size - 1;
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    return queue;
}

bool atom_push(atom_queue *queue, uint8_t data) {
    if (queue == NULL || queue->buf == NULL) return false;
    uint64_t head = load(&queue->head);
    uint64_t next = (((uint64_t)(head + 1U)) & queue->mask);
    if (next == load(&queue->tail)) return false;
    *(&queue->buf[head]) = data;
    store(&queue->head, next);
    return true;
}

int atom_pop(atom_queue *queue) {
    if (queue == NULL || queue->buf == NULL) return -1;
    uint64_t tail = load(&queue->tail);
    if (tail == load(&queue->head)) return -1;
    uint8_t data = queue->buf[tail];
    store(&queue->tail, (((uint64_t)(tail + 1U)) & queue->mask));
    return (int)data;
}

void free_queue(atom_queue *queue) {
    if (queue == NULL) return;
    free(queue->buf);
    free(queue);
}

atom_queue_mpmc *create_atom_queue_mpmc(uint64_t size) {
    if (size == 0 || (size & (size - 1)) != 0) { return NULL; }
    atom_queue_mpmc *queue = (atom_queue_mpmc *)malloc(sizeof(atom_queue_mpmc));
    if (!queue) return NULL;
    memset(queue, 0, sizeof(atom_queue_mpmc));
    queue->buf = (uint8_t *)malloc(size * sizeof(uint8_t));
    if (!queue->buf) {
        free(queue);
        return NULL;
    }
    queue->mask = size - 1;
    queue->head = 0;
    queue->tail = 0;
    queue->size = size;

    return queue;
}

bool atom_push_mpmc(atom_queue_mpmc *queue, uint8_t data) {
    if (queue == NULL) return false;
    while (true) {
        uint64_t head = load(&queue->head);
        uint64_t tail = load(&queue->tail);
        uint64_t next = (head + 1) & queue->mask;
        if (next == tail) { return false; }
        if (cas(&queue->head, head, next)) {
            queue->buf[head] = data;
            return true;
        }
    }
}

int atom_pop_mpmc(atom_queue_mpmc *queue) {
    if (queue == NULL) return -1;
    while (true) {
        uint64_t tail = load(&queue->tail);
        uint64_t head = load(&queue->head);
        if (tail == head) { return -1; }
        uint64_t next = (tail + 1) & queue->mask;
        if (cas(&queue->tail, tail, next)) {
            uint8_t data = queue->buf[tail];
            return data;
        }
    }
}

void free_queue_mpmc(atom_queue_mpmc *queue) {
    if (queue == NULL) return;
    free(queue->buf);
    free(queue);
}
