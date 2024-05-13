#ifndef CRASHPOWEROS_PCAT_H
#define CRASHPOWEROS_PCAT_H

#include "task.h"

struct pcat_process{
    uint32_t line;
    uint32_t chars;
    uint32_t buf_x,buf_y;
    uint32_t buffer_screen;
    int keys;
    struct File *file;
};

void pcat_launch(struct File *file);

#endif
