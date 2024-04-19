#ifndef CRASHPOWEROS_PCAT_H
#define CRASHPOWEROS_PCAT_H

#include "task.h"
#include "fat16.h"

struct pcat_process{
    uint8_t is_e;
};

void pcat_launch(struct task_struct *father,struct File *file);

#endif
