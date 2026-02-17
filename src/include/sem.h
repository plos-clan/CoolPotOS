#pragma once

#include "types.h"
#include "lock.h"

typedef struct sem {
    spin_t lock;
    uint32_t cnt;
    bool invalid;
} sem_t;

bool sem_wait(sem_t *sem, uint32_t timeout);
void sem_post(sem_t *sem);
