#pragma once

#include "ctype.h"
#include "lock.h"

typedef struct Queue {
    uint8_t   *buf;
    uint64_t   mask;
    uint64_t   head;
    uint64_t   tail;
    uint64_t   size;
    uint64_t   length;
    ticketlock lock;
} atom_queue;

atom_queue *create_atom_queue(uint64_t size);
bool        atom_push(atom_queue *queue, uint8_t data);
uint8_t     atom_pop(atom_queue *queue);
void        free_queue(atom_queue *queue);
