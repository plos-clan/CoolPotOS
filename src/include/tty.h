#ifndef CRASHPOWEROS_TTY_H
#define CRASHPOWEROS_TTY_H

#include "common.h"
#include "fifo.h"

typedef struct tty{
    uint32_t *frame_buffer; // 图形缓冲区映射
    uint32_t width;
    uint32_t height;
    bool is_default_frame;  // 是否为根缓冲区
    struct FIFO8 *fifo;     // 键盘输出缓冲区
}tty_t;

#include "task.h"

void init_default_tty(struct task_struct *task);
void free_tty(struct task_struct *task);

#endif
