#pragma once

#define IPC_MSG_TYPE_NONE     0
#define IPC_MSG_TYPE_EXIT     1 // 进程终止
#define IPC_MSG_TYPE_KEYBOARD 2 // 键盘输入
#define IPC_MSG_TYPE_MOUSE    3 // 鼠标输入
#define IPC_MSG_TYPE_TIMER    4 // 时钟计数
#define IPC_MSG_TYPE_EPID     5 // 子进程退出信号
#define IPC_MSG_TYPE_EXEC     6 // execve 调用信号

#define IPC_QUEUE_CAPACITY 256

#include "lock.h"
#include "types.h"

typedef struct ipc_message *ipc_message_t;

struct ipc_message {
    pid_t pid;        // 发送方PID
    uint8_t type;     // 消息类型
    uint8_t data[64]; // 数据
    size_t index;     // 消息队列索引
};

typedef struct ipc_queue_t {
    spin_t lock;
    void *items[IPC_QUEUE_CAPACITY];
    size_t capacity;     // 最大容量
    size_t head;         // 头部索引（下一个出队位置）
    size_t tail;         // 尾部索引（下一个入队位置）
    _Atomic size_t size; // 当前队列元素数量
} ipc_queue_t;

/**
 * 向指定进程发送消息
 * @param process 进程控制块
 * @param message 消息
 */
void ipc_send(ipc_queue_t *queue, ipc_message_t message);

/**
 * 接受指定类型的消息
 * @param type 类型
 * @return == NULL ? 无消息 : 消息
 */
ipc_message_t ipc_recv(ipc_queue_t *queue, uint8_t type);

/**
 * 等待指定类型的消息
 * @param type 类型
 * @return 消息
 */
ipc_message_t ipc_recv_wait(ipc_queue_t *queue, uint8_t type);

ipc_message_t ipc_recv_wait2(ipc_queue_t *queue, uint8_t type0, uint8_t type1);

/**
 * 释放所有指定类型的消息
 * @param type 类型
 */
void ipc_free_type(ipc_queue_t *queue, uint8_t type);

ipc_queue_t *ipc_queue_init();
void ipc_queue_release(ipc_queue_t *queue);
