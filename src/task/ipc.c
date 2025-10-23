#include "task/ipc.h"
#include "task/scheduler.h"

ipc_queue_t *ipc_queue_init() {
    ipc_queue_t *queue = malloc(sizeof(ipc_queue_t));
    queue->lock = SPIN_INIT;
    queue->capacity = IPC_QUEUE_CAPACITY;
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    return queue;
}

static void* ipc_queue_dequeue(ipc_queue_t *queue) {
    void* item = NULL;
    spin_lock(queue->lock);
    if (queue->size == 0) {
        spin_unlock(queue->lock);
        return NULL;
    }
    item = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;
    spin_unlock(queue->lock);
    return item;
}

static size_t ipc_queue_enqueue(ipc_queue_t *queue, void *item) {
    size_t index = SIZE_MAX;
    spin_lock(queue->lock);
    if (queue->size == queue->capacity) {
        return index;
    }
    index = queue->tail;
    queue->items[queue->tail] = item;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;
    return index;
}

void ipc_send(ipc_queue_t *queue, ipc_message_t message) {
    if (queue == NULL || message == NULL) { return; }
    size_t index   = ipc_queue_enqueue(queue, message);
    message->index = index;
    spin_unlock(queue->lock);
}

USED void ipc_free_type(ipc_queue_t *queue,uint8_t type) {
    for (size_t i = 0; i < queue->size; ++i) {
        ipc_message_t message = (ipc_message_t)ipc_queue_dequeue(queue);
        if (message->type == type) {
            free(message);
        } else {
            message->index = ipc_queue_enqueue(queue, message);
        }
        spin_unlock(queue->lock);
    }
}

ipc_message_t ipc_recv(ipc_queue_t *queue,uint8_t type) {
    for (size_t i = 0; i < queue->size; ++i) {
        ipc_message_t message = (ipc_message_t)ipc_queue_dequeue(queue);
        if (message->type == type) { return message; }
        message->index = ipc_queue_enqueue(queue, message);
        spin_unlock(queue->lock);
    }
    return NULL;
}

ipc_message_t ipc_recv_wait(ipc_queue_t *queue,uint8_t type) {
    ipc_message_t message = NULL;
    do {
        arch_pause();
        scheduler_yield();
        message = ipc_recv(queue,type);
    } while (message == NULL);
    return message;
}

ipc_message_t ipc_recv_wait2(ipc_queue_t *queue,uint8_t type0, uint8_t type1) {
    ipc_message_t message = NULL;
    do {
        __asm__ volatile("pause");
        scheduler_yield();
        message = ipc_recv(queue,type0);
        if (message == NULL) { message = ipc_recv(queue,type1); }
    } while (message == NULL);
    return message;
}

void ipc_queue_release(ipc_queue_t *queue) {
    if (queue == NULL) {
        return;
    }
    spin_lock(queue->lock);
    size_t current_index = queue->head;
    size_t count = queue->size;
    for (size_t i = 0; i < count; i++) {
        ipc_message_t *message = (ipc_message_t *)queue->items[current_index];

        if (message != NULL) {
            free(message);
            queue->items[current_index] = NULL;
        }
        current_index = (current_index + 1) % queue->capacity;
    }
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    spin_unlock(queue->lock);
    free(queue);
}
