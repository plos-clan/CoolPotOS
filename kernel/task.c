#include "../include/task.h"
#include "../include/common.h"
#include "../include/graphics.h"

struct task_struct *running_proc_head = NULL;
struct task_struct *wait_proc_head = NULL;
struct task_struct *current = NULL;

extern page_directory_t *kernel_directory;

extern void switch_to(struct context *prev, struct context *next);

int now_pid = 0;

void print_proc_t(int *i,struct task_struct *base,struct task_struct *cur){
    if(cur->pid == base->pid){
        switch (cur->state) {
            case TASK_RUNNABLE:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Running");
                break;
            case TASK_SLEEPING:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Sleeping");
                break;
            case TASK_UNINIT:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Init");
                break;
            case TASK_ZOMBIE:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Zombie");
                break;
            case TASK_DEATH:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Death");
                break;
        }
        (*i)++;
    } else{
        switch (cur->state) {
            case TASK_RUNNABLE:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Running");
                break;
            case TASK_SLEEPING:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Sleeping");
                break;
            case TASK_UNINIT:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Init");
                break;
            case TASK_ZOMBIE:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Zombie");
                break;
            case TASK_DEATH:
                printf("%s      %d     %s\n",cur->name,cur->pid,"Death");
                break;
        }
        (*i)++;
        print_proc_t(i,base,cur->next);
    }
}

void print_proc(){
    printf("====--------[Processes]---------===\n");
    int index = 0;
    print_proc_t(&index,current,current->next);
    printf("Name          Pid     Status [All Proc: %d]\n\n",index);
}

void schedule() {
    if (current) {
        volatile task_state state = current->next->state;
        if(state == TASK_RUNNABLE || state == TASK_SLEEPING ){
            change_task_to(current->next);
        }
    }
}

void change_task_to(struct task_struct *next) {
    if (current != next) {
        struct task_struct *prev = current;
        current = next;
        //switch_page_directory(current->pgd_dir);
        switch_to(&(prev->context), &(current->context));
    }
}

int32_t kernel_thread(int (*fn)(void *), void *arg,char* name) {
    struct task_struct *new_task = (struct task_struct *) kmalloc(STACK_SIZE);
    assert(new_task != NULL, "kern_thread: kmalloc error");

    // 将栈低端结构信息初始化为 0
    memset(new_task,0, sizeof(struct task_struct));

    new_task->state = TASK_RUNNABLE;
    new_task->stack = current;
    new_task->pid = now_pid++;
    new_task->pgd_dir = (page_directory_t *) kmalloc_a(sizeof(page_directory_t));

    new_task->name = name;

    uint32_t *stack_top = (uint32_t * )((uint32_t) new_task + STACK_SIZE);

    *(--stack_top) = (uint32_t) arg;
    *(--stack_top) = (uint32_t) kthread_exit;
    *(--stack_top) = (uint32_t) fn;

    new_task->context.esp = (uint32_t) new_task + STACK_SIZE - sizeof(uint32_t) * 3;

    // 设置新任务的标志寄存器未屏蔽中断，很重要
    new_task->context.eflags = 0x200;
    new_task->next = running_proc_head;

    // 找到当前进任务队列，插入到末尾
    struct task_struct *tail = running_proc_head;
    assert(tail != NULL, "Must init sched!");

    while (tail->next != running_proc_head) {
        tail = tail->next;
    }
    tail->next = new_task;

    return new_task->pid;
}

void kthread_exit() {
    register uint32_t val asm ("eax");
    printf("Task exited with value %d\n", val);
    current->state = TASK_DEATH;
    while (1);
}

void init_sched() {
    // 为当前执行流创建信息结构体 该结构位于当前执行流的栈最低端
    current = (struct task_struct *) kmalloc(sizeof(struct task_struct));

    current->state = TASK_RUNNABLE;
    current->pid = now_pid++;
    current->stack = current;   // 该成员指向栈低地址
    current->pgd_dir = kernel_directory;
    current->name = "CPOS-System";

    current->next = current;

    running_proc_head = current;
}