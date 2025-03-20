#include "tcb.h"
#include "io.h"
#include "krlibc.h"
#include "scheduler.h"
#include "user_malloc.h"

// 将新线程添加到调度链中
static void add_thread(pcb_t *pcb, tcb_t thread) {
    // 检查输入是否为空
    if (pcb == NULL || thread == NULL) return;

    // 获取主线程的尾部
    tcb_t tailt = pcb->main_thread_head;
    while (tailt->next != pcb->main_thread_head) {
        if (tailt->next == NULL) break;
        tailt = tailt->next;
    }
    // 将新线程添加到链表末尾
    tailt->next = thread;
}

// 结束并从调度链中移除指定用户线程
void kill_user_thread(pcb_t *pcb, tcb_t thread) {
    disable_scheduler();
    pcb->sche_time--;

    // 初始化头节点和前一个节点
    tcb_t head = pcb->main_thread_head;
    tcb_t last = NULL;

    infinite_loop {
        if (head->tid == thread->tid) {
            // 如果是第一个节点，直接连接前驱和后继
            if (last == NULL) {
                head = head->next;
            } else {
                last->next = head->next;
            }

            // 释放内存
            kfree(pcb);
            enable_scheduler();
            io_sti();
            return;
        }
        last = head;
        head = head->next;
    }
}

// 创建一个新的内核子线程
tcb_t create_kernel_subthread(pcb_t *pcb, void *func) {
    // 分配线程对象和栈空间
    tcb_t thread = kmalloc(STACK_SIZE);

    // 初始化线程属性
    thread->next         = NULL;
    thread->father_pcb   = pcb;
    thread->tid          = pcb->now_tid++;
    thread->kernel_stack = thread;
    thread->user_stack   = pcb->user_stack;

    // 更新调度时间片计数器
    pcb->sche_time++;

    // 设置堆栈和返回地址
    uint32_t *stack_top = (uint32_t *)((uint32_t)thread->kernel_stack + STACK_SIZE);
    *(--stack_top)      = (uint32_t)process_exit;
    *(--stack_top)      = (uint32_t)func;

    // 设置上下文寄存器
    thread->context.esp    = (uint32_t)thread + STACK_SIZE - sizeof(uint32_t) * 3;
    thread->context.eflags = 0x200;

    // 将线程添加到链表中
    if (pcb->main_thread_head == NULL) {
        pcb->current_thread = pcb->main_thread_head = thread;
    } else
        add_thread(pcb, thread);

    return thread;
}

// 创建一个新的用户线程
tcb_t create_user_thread(pcb_t *pcb, void *func) {
    // 分配内核栈和线程对象
    tcb_t thread = kmalloc(STACK_SIZE);

    // 初始化线程属性
    thread->next         = NULL;
    thread->father_pcb   = pcb;
    thread->tid          = pcb->now_tid++;
    thread->kernel_stack = thread;
    thread->user_stack   = pcb->user_stack;

    // 设置用户模式切换和返回地址
    uint32_t *stack_top = (uint32_t *)((uint32_t)thread->kernel_stack + STACK_SIZE);
    *(--stack_top)      = (uint32_t)func;
    *(--stack_top)      = (uint32_t)process_exit;
    *(--stack_top)      = (uint32_t)switch_to_user_mode;

    // 设置上下文寄存器
    thread->context.esp    = (uint32_t)thread + STACK_SIZE - sizeof(uint32_t) * 3;
    thread->context.eflags = 0x200;

    // 将线程添加到链表中
    if (pcb->main_thread_head == NULL) {
        pcb->current_thread = pcb->main_thread_head = thread;
    } else
        add_thread(pcb, thread);

    return thread;
}
