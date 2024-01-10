#include "../include/thread.h"
#include "../include/kheap.h"

ThreadManager *manager;

void register_set(registers_t reg){

}

void thread_setup(){
    manager = (ThreadManager*) alloc(sizeof(ThreadBlock));
    manager->num_thread = 0;
    manager->current_thread_index = 0;
    manager->blocks = (ThreadBlock**)alloc(sizeof (ThreadBlock) * MAX_THREAD);
    manager->current_thread = NULL;
}

void switch_thread(ThreadBlock *prev_thread, ThreadBlock *next_thread) {
    // 保存当前线程的上下文
    asm volatile (
            "pusha\n\t"
            "mov %%esp, %0\n\t"
            : "=m" (prev_thread->reg->esp)
            :
            );
    // 切换到下一个线程的上下文
    asm volatile (
            "mov %0, %%esp\n\t"
            "popa\n\t"
            "ret\n\t"
            :
            : "m" (next_thread->reg->esp)
            );
}

void schedule(registers_t reg) {
    manager->current_thread_index = (manager->current_thread_index + 1) % manager->num_thread;
    switch_thread(manager->current_thread, manager->blocks[manager->current_thread_index]);
}

ThreadBlock* create_thread(void (*entry_point)()) {
    ThreadBlock *new_thread = (ThreadBlock*) alloc(sizeof(ThreadBlock));
    void *new_stack = alloc(sizeof(1024));

    init_thread(new_thread, entry_point, new_stack);

    manager->blocks[manager->num_thread++] = new_thread;

    return new_thread;
}

void start_thread(ThreadBlock *block){
    get_reg();
}

void init_thread(ThreadBlock *block,void (*entry_point)(),void *stack){
    block->reg->eip = entry_point;
    block->reg->esp = stack;
    block->status = INIT;
}