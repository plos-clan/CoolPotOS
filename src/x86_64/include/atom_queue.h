#pragma once

#include "ctype.h"
#include "lock.h"

typedef struct {
    uint8_t *buf;
    uint64_t mask;
    volatile uint64_t head;
    volatile uint64_t tail;
    uint64_t size;
} atom_queue;

atom_queue *create_atom_queue(uint64_t size);
bool        atom_push(atom_queue *queue, uint8_t data);
int         atom_pop(atom_queue *queue);
void        free_queue(atom_queue *queue);

typedef struct {
    uint8_t *buf;
    uint64_t mask;
    volatile uint64_t head;
    volatile uint64_t tail;
    uint64_t size;
} atom_queue_mpmc;

atom_queue_mpmc *create_atom_queue_mpmc(uint64_t size);
bool atom_push_mpmc(atom_queue_mpmc *queue, uint8_t data);
int atom_pop_mpmc(atom_queue_mpmc *queue);
void free_queue_mpmc(atom_queue_mpmc *queue);
