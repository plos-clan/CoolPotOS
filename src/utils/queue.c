#include "queue.h"
#include "alloc.h"

bool is_queue_empty(queue_t *queue){
    return queue->size == 0;
}

void add_queue(queue_t * queue, int value) {
    struct queue_node* newNode = (struct queue_node*)malloc(sizeof(struct queue_node));
    newNode->data = value;
    newNode->next = NULL;
    if (is_queue_empty(queue)) {
        queue->head = queue->end = newNode;
    } else {
        queue->end->next = newNode;
        queue->end = newNode;
    }
    queue->size++;
}

int out_queue(queue_t * queue) {
    if (is_queue_empty(queue)) {
        return -1;
    }
    struct queue_node* temp = queue->head;
    int value = temp->data;
    queue->end = queue->end->next;
    if (!queue->head) {
        queue->end = NULL;
    }
    free(temp);
    queue->size--;
    return value;
}

void destroy_queue(queue_t * queue) {
    while (!is_queue_empty(queue)) {
        out_queue(queue);
    }
    free(queue);
}

queue_t *create_queue(){
    queue_t *queue = malloc(sizeof(queue_t));
    queue->size = 0;
    queue->head = queue->end = NULL;
    return queue;
}

