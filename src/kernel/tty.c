#include "../include/tty.h"

extern uint32_t *screen;
extern uint32_t width, height;

void init_default_tty(struct task_struct *task){
    task->tty->fifo = kmalloc(sizeof(struct FIFO8));
    char* buffer = kmalloc(256);

    task->tty->frame_buffer = screen;
    task->tty->width = width;
    task->tty->height = height;
    task->tty->is_default_frame = true;

    fifo8_init(task->tty->fifo,256,buffer);
}

void free_tty(struct task_struct *task){
    if(!task->tty->is_default_frame){
        kfree(task->tty->frame_buffer);
    }
    kfree(task->tty->fifo->buf);
    kfree(task->tty->fifo);
    kfree(task->tty);
}
