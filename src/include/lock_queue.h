#pragma once

#include "lock.h"

typedef struct LockNode {
    void *data;
    struct LockNode *next;
    int index;
} lock_node;

typedef struct {
    lock_node *head;
    lock_node *tail;
    ticketlock lock;
    ticketlock iter_lock;
    int size;
    int next_index;
} lock_queue;

/**
 * 初始化一个有锁可迭代队列
 * @return 队列
 */
lock_queue *queue_init();

/**
 * 入队操作
 * @param q 队列
 * @param data 存储的句柄
 * @return 该节点的索引
 */
int queue_enqueue(lock_queue *q, void *data);

/**
 * 删除指定索引的节点
 * @param q 队列
 * @param index 索引
 * @return 被删除节点的句柄
 */
void *queue_remove_at(lock_queue *q, int index);

/**
 * 出队操作
 * @param q 队列
 * @return 该节点存储的句柄
 */
void *queue_dequeue(lock_queue *q);

/**
 * 销毁队列
 * @param q 队列
 */
void queue_destroy(lock_queue *q);

/**
 * 传入回调函数迭代指定队列
 * @param q 队列
 * @param callback 回调函数
 * @param argument 回调函数传入的参数
 */
void queue_iterate(lock_queue *q, void (*callback)(void *,void*), void* argument);
