#pragma once

struct queue_node {
    int data;
    struct queue_node *next;
};

typedef struct queue{
    struct queue_node *head;
    struct queue_node *end;
    int size;
}queue_t;

queue_t *create_queue();
bool is_queue_empty(queue_t *queue);
void add_queue(queue_t * queue, int value);
int out_queue(queue_t * queue);
void destroy_queue(queue_t * queue);
