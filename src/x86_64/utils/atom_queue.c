#include "atom_queue.h"
#include "heap.h"
#include "io.h"
#include "krlibc.h"

atom_queue *create_atom_queue(uint64_t size) {
    atom_queue *queue = (atom_queue *)malloc(sizeof(atom_queue));
    memset(queue, 0, sizeof(atom_queue));
    queue->buf    = (uint8_t *)malloc(size * sizeof(uint8_t));
    queue->mask   = size - 1;
    queue->head   = 0;
    queue->tail   = 0;
    queue->size   = size;
    queue->length = 0;
    return queue;
}

bool atom_push(atom_queue *queue, uint8_t data) {
    if (queue == NULL) return false;
    //ticket_lock(&queue->lock);
    uint64_t head = load(&queue->head);
    uint64_t next = (head + 1) & queue->mask;
    if (next == load(&queue->tail)) return false;
    *(&queue->buf[head]) = data;
    store(&queue->head, next);
    queue->length++;
    // ticket_unlock(&queue->lock);
    return true;
}

uint8_t atom_pop(atom_queue *queue) {
    if (queue == NULL) return -1;
    if (queue->length <= 0) return -1;
    // ticket_lock(&queue->lock);
    uint64_t tail = load(&queue->tail);
    if (tail == load(&queue->head)) return -1;
    uint8_t data = queue->buf[tail];
    store(&queue->tail, (tail + 1) & queue->mask);
    queue->length--;
    //ticket_unlock(&queue->lock);
    return data;
}

void free_queue(atom_queue *queue) {
    if (queue == NULL) return;
    free(queue->buf);
    free(queue);
}
