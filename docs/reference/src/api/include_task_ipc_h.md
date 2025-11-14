# include/task/ipc.h

## `void ipc_send(ipc_queue_t *queue, ipc_message_t message);`


向指定进程发送消息

- **`process`**: 进程控制块
- **`message`**: 消息


---

## `ipc_message_t ipc_recv(ipc_queue_t *queue, uint8_t type);`


接受指定类型的消息

- **`type`**: 类型
- **Returns**: == NULL ? 无消息 : 消息


---

## `ipc_message_t ipc_recv_wait(ipc_queue_t *queue, uint8_t type);`


等待指定类型的消息

- **`type`**: 类型
- **Returns**: 消息


---

## `void ipc_free_type(ipc_queue_t *queue, uint8_t type);`


释放所有指定类型的消息

- **`type`**: 类型


---

