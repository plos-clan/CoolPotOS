#pragma once

#include "task.h"

void futex_init();
void futex_add(void *phys_addr, tcb_t thread);
void futex_wake(void *phys_addr, int count);
void futex_free(tcb_t thread);
