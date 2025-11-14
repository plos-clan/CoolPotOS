#include "ipc.h"
#include "heap.h"

void ipc_send(pcb_t process, ipc_message_t message) {
    if (process == NULL || message == NULL) { return; }
    if (process->ipc_queue == NULL) { return; }
    size_t index   = lock_queue_enqueue(process->ipc_queue, message);
    message->index = index;
    spin_unlock(process->ipc_queue->lock);
}

USED void ipc_free_type(uint8_t type) {
    pcb_t pcb = get_current_task()->parent_group;
    for (size_t i = 0; i < pcb->ipc_queue->size; ++i) {
        ipc_message_t message = (ipc_message_t)queue_dequeue(pcb->ipc_queue);
        if (message->type == type) {
            free(message);
        } else {
            message->index = lock_queue_enqueue(pcb->ipc_queue, message);
        }
        spin_unlock(pcb->ipc_queue->lock);
    }
}

ipc_message_t ipc_recv(uint8_t type) {
    pcb_t pcb = get_current_task()->parent_group;
    for (size_t i = 0; i < pcb->ipc_queue->size; ++i) {
        ipc_message_t message = (ipc_message_t)queue_dequeue(pcb->ipc_queue);
        if (message->type == type) { return message; }
        message->index = lock_queue_enqueue(pcb->ipc_queue, message);
        spin_unlock(pcb->ipc_queue->lock);
    }
    return NULL;
}

ipc_message_t ipc_recv_wait(uint8_t type) {
    ipc_message_t message = NULL;
    do {
        __asm__ volatile("pause");
        scheduler_yield();
        message = ipc_recv(type);
    } while (message == NULL);
    return message;
}

ipc_message_t ipc_recv_wait2(uint8_t type0, uint8_t type1) {
    ipc_message_t message = NULL;
    do {
        __asm__ volatile("pause");
        scheduler_yield();
        message = ipc_recv(type0);
        if (message == NULL) { message = ipc_recv(type1); }
    } while (message == NULL);
    return message;
}
