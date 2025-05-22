#pragma once

#define IPC_MSG_TYPE_NONE     0
#define IPC_MSG_TYPE_EXIT     1 // 进程终止
#define IPC_MSG_TYPE_KEYBOARD 2 // 键盘输入
#define IPC_MSG_TYPE_MOUSE    3 // 鼠标输入
#define IPC_MSG_TYPE_TIMER    4 // 时钟计数
#define IPC_MSG_TYPE_EPID     5 // 子进程退出信号

#include "ctype.h"
#include "lock_queue.h"
#include "pcb.h"

typedef struct ipc_message *ipc_message_t;

struct ipc_message {
    int     pid;      // 发送方PID
    uint8_t type;     // 消息类型
    uint8_t data[64]; // 数据
    size_t  index;    // 消息队列索引
};

/**
 * 向指定进程发送消息
 * @param process 进程控制块
 * @param message 消息
 */
void ipc_send(pcb_t process, ipc_message_t message);

/**
 * 接受指定类型的消息
 * @param type 类型
 * @return == NULL ? 无消息 : 消息
 */
ipc_message_t ipc_recv(uint8_t type);

/**
 * 等待指定类型的消息
 * @param type 类型
 * @return 消息
 */
ipc_message_t ipc_recv_wait(uint8_t type);

/**
 * 释放所有指定类型的消息
 * @param type 类型
 */
void ipc_free_type(uint8_t type);
