#ifndef CPOS_QUEUE_H
#define CPOS_QUEUE_H

typedef struct queue_node {
    char data;
    struct queue_node *next;
}QNode;

typedef struct queue {
    QNode *phead; //队头
    QNode *ptail; //队尾
    int size;
}Queue;

Queue *create_queue();
void free_queue(Queue *queue);
char queue_pop(Queue *pq);
void queue_push(Queue *pq, char x);

#endif
