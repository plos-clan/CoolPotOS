#ifndef CRASHPOWEROS_HEAP_H
#define CRASHPOWEROS_HEAP_H

#include "task.h"

void *user_alloc(struct task_struct *user_t, size_t size);
void use_free(struct task_struct *user_t,void *block);
void *use_sbrk(struct task_struct *user_t,int incr);

#endif
