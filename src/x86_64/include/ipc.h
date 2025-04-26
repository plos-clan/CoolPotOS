#pragma once

#define IPC_MSG_TYPE_NONE 0
#define IPC_MSG_TYPE_EXIT 1
#define IPC_MSG_TYPE_KEYBOARD 2
#define IPC_MSG_TYPE_MOUSE 3
#define IPC_MSG_TYPE_TIMER 4

#include "ctype.h"
#include "lock_queue.h"
#include "pcb.h"

typedef struct ipc_message *ipc_message_t;

struct ipc_message{
    size_t  pid;
    uint8_t type;
    uint8_t data[64];
    size_t  index;
};

void ipc_send(pcb_t process, ipc_message_t message);
ipc_message_t ipc_recv(uint8_t type);
ipc_message_t ipc_recv_wait(uint8_t type);
