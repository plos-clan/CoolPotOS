#include "../include/task.h"
#include "../include/common.h"
#include "../include/graphics.h"
#include "../include/io.h"
#include "../include/description_table.h"

struct task_struct *running_proc_head = NULL;
struct task_struct *wait_proc_head = NULL;
struct task_struct *current = NULL;

extern page_directory_t *kernel_directory;

extern void switch_to(struct context *prev, struct context *next);

int now_pid = 0;

struct task_struct* get_current(){
    return current;
}

void print_proc_t(int *i,struct task_struct *base,struct task_struct *cur,int is_print){
    if(cur->pid == base->pid){
        if(is_print){
            switch (cur->state) {
                case TASK_RUNNABLE:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Running");
                    break;
                case TASK_SLEEPING:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Sleeping");
                    break;
                case TASK_UNINIT:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Init");
                    break;
                case TASK_ZOMBIE:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Zombie");
                    break;
                case TASK_DEATH:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Death");
                    break;
            }
        }
        (*i)++;
    } else{
        if(is_print){
            switch (cur->state) {
                case TASK_RUNNABLE:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Running");
                    break;
                case TASK_SLEEPING:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Sleeping");
                    break;
                case TASK_UNINIT:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Init");
                    break;
                case TASK_ZOMBIE:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Zombie");
                    break;
                case TASK_DEATH:
                    printf("%-17s      %-2d     %s\n",cur->name,cur->pid,"Death");
                    break;
            }
        }
        (*i)++;
        print_proc_t(i,base,cur->next,is_print);
    }
}

int get_procs(){
    int index = 0;
    print_proc_t(&index,current,current->next,0);
    return index;
}

void print_proc(){
    int index = 0;
    print_proc_t(&index,current,current->next,1);
    printf("====---------------[Processes]----------------====\n");
    printf("Name                  Pid     Status [All Proc: %d]\n\n",index);
}

static void found_task(int pid,struct task_struct *head,struct task_struct *base,struct task_struct **argv,int first){
    struct task_struct *t = base;
    if(t == NULL){
        argv = NULL;
        return;
    }
    if(t->pid == pid){
        *argv = t;
        return;
    } else{
        if(!first)
            if(head->pid == t->pid){
                argv = NULL;
                return;
            }
        found_task(pid,head,t->next,argv,0);
    }
}

struct task_struct* found_task_pid(int pid){
    struct task_struct *argv = NULL;
    found_task(pid,running_proc_head,running_proc_head,&argv,1);
    if(argv == NULL){
        printf("Cannot found task Pid:[%d].\n",pid);
        return NULL;
    }
    return argv;
}

void wait_task(struct task_struct *task){
    task->state = TASK_SLEEPING;
}

void start_task(struct  task_struct *task){
    task->state = TASK_RUNNABLE;
}

void task_kill(int pid){
    struct task_struct *argv = found_task_pid(pid);
    if(argv == NULL){
        printf("Cannot found task Pid:[%d].\n",pid);
        return;
    }
    if(argv->pid == 0){
        printf("[\033kernel\036]: Taskkill cannot terminate kernel processes.\n");
        return;
    }
    argv->state = TASK_DEATH;
    printf("Taskkill process PID:%d Name:%s\n", current->pid, current->name);
    printf("Task [%s] exit code: -130.\n",argv->name);
    io_sti();
    kfree(argv);
    struct task_struct *head = running_proc_head;
    struct task_struct *last = NULL;
    while (1){
        if(head->pid == argv->pid){
            last->next = argv->next;
            return;
        }
        last = head;
        head = head->next;
    }
    io_cli();
}

void schedule() {
    if (current) {
        if(current->next->state == TASK_SLEEPING){
            change_task_to(current->next->next);
            return;
        }
        change_task_to(current->next);
    }
}

void change_task_to(struct task_struct *next) {
    if (current != next) {
        struct task_struct *prev = current;
        current = next;

        page_switch(current->pgd_dir);
        set_kernel_stack(current->stack);
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
    new_task->pgd_dir = clone_directory(current->pgd_dir) ;

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

void kill_all_task(){
    struct task_struct *head = running_proc_head;
    while (1){
        head = head->next;
        if(head == NULL || head->pid == running_proc_head->pid){
            return;
        }
        if(head->pid == current->pid) continue;
        task_kill(head->pid);
    }
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