#ifndef CPOS_THREAD_H
#define CPOS_THREAD_H

#include <stdint.h>
#include "isr.h"

#define INIT 0x00
#define RUNNING 0x01
#define INTERRUPT 0x02
#define DEATH 0x03

#define MAX_THREAD 100

typedef struct {
    registers_t *reg;
    int status;
}ThreadBlock;

typedef struct {
    int num_thread;
    ThreadBlock **blocks;
    int current_thread_index;
    ThreadBlock *current_thread;
}ThreadManager;

void register_set(registers_t reg);
void get_reg(); //ASM function
void thread_setup();
void start_thread(ThreadBlock *block);
void schedule();
ThreadBlock* create_thread(void (*entry_point)());
void init_thread(ThreadBlock *block,void (*entry_point)(),void *stack_point);
void switch_thread(ThreadBlock *prev_thread, ThreadBlock *next_thread);

#endif
