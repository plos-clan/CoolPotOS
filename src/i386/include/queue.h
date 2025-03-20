#pragma once

#include "ctypes.h"
#include "kmalloc.h"

#pragma GCC system_header

#ifdef ALL_IMPLEMENTATION
#  define QUEUE_IMPLEMENTATION
#endif

/**
 *\struct Node
 *\brief 队列节点结构
 */
typedef struct queue_node *queue_node_t;
struct queue_node {
    void        *data; /**< 节点数据 */
    queue_node_t next; /**< 下一个节点指针 */
};

#ifdef QUEUE_IMPLEMENTATION
#  define extern static
#endif

/**
 *\struct Queue
 *\brief 队列结构
 */
typedef struct queue {
    queue_node_t head; /**< 队列前端指针 */
    queue_node_t tail; /**< 队列后端指针 */
    size_t       size; /**< 队列长度 */
} *queue_t;

extern void queue_init(queue_t queue);

extern void queue_deinit(queue_t queue);

/**
 *\brief 创建一个新的队列
 *\return 新的队列指针
 */
extern queue_t queue_alloc();

/**
 *\brief 销毁队列并释放内存
 *\param[in] queue 队列指针
 */
extern void queue_free(queue_t queue);

/**
 *\brief 将元素入队列
 *\param[in] queue 队列指针
 *\param[in] data 入队的元素
 */
extern void queue_enqueue(queue_t queue, void *data);

/**
 *\brief 将元素出队列
 *\param[in] queue 队列指针
 *\return 出队的元素
 */
extern void *queue_dequeue(queue_t queue);

/**
 *\brief 判断队列是否为空
 *\param[in] queue 队列指针
 *\return 若队列为空，则返回true，否则返回false
 */
extern bool queue_isempty(queue_t queue);

/**
 *\brief 获取队列的长度
 *\param[in] queue 队列指针
 *\return 队列的长度
 */
extern size_t queue_size(queue_t queue);

/**
 *\brief 打印队列中的元素
 *\param[in] queue 队列指针
 */
extern void queue_print(queue_t queue);

#ifdef QUEUE_IMPLEMENTATION
#  undef extern
#endif

#ifdef QUEUE_IMPLEMENTATION

static void queue_init(queue_t queue) {
  if (queue == NULL) return;
  queue->head = NULL;
  queue->tail = NULL;
  queue->size = 0;
}

static void queue_deinit(queue_t queue) {
  if (queue == NULL) return;

  queue_node_t current = queue->head;
  while (current != NULL) {
    queue_node_t temp = current;
    current           = current->next;
    kfree(temp);
  }

  queue->head = NULL;
  queue->tail = NULL;
  queue->size = 0;
}

static queue_t queue_alloc() {
  queue_t queue = kmalloc(sizeof(*queue));
  queue_init(queue);
  return queue;
}

static void queue_free(queue_t queue) {
  if (queue == NULL) return;

  queue_node_t current = queue->head;
  while (current != NULL) {
    queue_node_t temp = current;
    current           = current->next;
    kfree(temp);
  }

  kfree(queue);
}

static void queue_free_with(queue_t queue, free_t callback) {
  if (queue == NULL) return;

  queue_node_t current = queue->head;
  while (current != NULL) {
    queue_node_t temp = current;
    if (callback) callback(current->data);
    current = current->next;
    kfree(temp);
  }

  kfree(queue);
}

static void queue_enqueue(queue_t queue, void *data) {
  if (queue == NULL) return;

  queue_node_t node = kmalloc(sizeof(*node));
  node->data        = data;
  node->next        = NULL;

  if (queue->head == NULL) {
    queue->head = node;
  } else {
    queue->tail->next = node;
  }

  queue->tail = node;
  queue->size++;
}

static void *queue_dequeue(queue_t queue) {
  if (queue == NULL) return NULL;
  if (queue->head == NULL) return NULL;

  queue_node_t temp = queue->head;
  void        *data = temp->data;
  queue->head       = queue->head->next;
  kfree(temp);

  if (queue->head == NULL) queue->tail = NULL;

  queue->size--;
  return data;
}

static bool queue_isempty(queue_t queue) {
  return queue == NULL || queue->head == NULL;
}

static size_t queue_size(queue_t queue) {
  return (queue == NULL) ? 0 : queue->size;
}

#  ifdef __libplos__
static void queue_print(queue_t queue) {
  queue_node_t current = queue->head;
  while (current != NULL) {
    printf("%p -> ", current->data);
    current = current->next;
  }
  printf("NULL\n");
}
#  endif

#  undef QUEUE_IMPLEMENTATION
#endif
