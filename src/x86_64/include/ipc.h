#pragma once

#define IPC_MSG_TYPE_NONE     0
#define IPC_MSG_TYPE_EXIT     1
#define IPC_MSG_TYPE_KEYBOARD 2
#define IPC_MSG_TYPE_MOUSE    3
#define IPC_MSG_TYPE_TIMER    4

#include "ctype.h"
#include "lock_queue.h"
#include "pcb.h"

typedef struct ipc_message *ipc_message_t;

struct ipc_message {
    size_t  pid;
    uint8_t type;
    uint8_t data[64];
    size_t  index;
};

/**
 * 向指定进程发送消息
 * @param process 进程控制块
 * @param message 消息
 */
void          ipc_send(pcb_t process, ipc_message_t message);

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
void          ipc_free_type(uint8_t type);
