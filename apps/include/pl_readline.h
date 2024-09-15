#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#if __x86_64__
typedef signed long long isize;
typedef unsigned long long usize;
#else
typedef signed long isize;
typedef unsigned long usize;
#endif
// ---------------------------------------

#define auto __auto_type
#define LIST_IMPLEMENTATION

/**
 *\struct ListNode
 *\brief 链表节点结构
 */
typedef struct list *list_t;
struct list {
  union {
    void *data;
    isize idata;
    usize udata;
  };
  list_t prev;
  list_t next;
};

#ifdef LIST_IMPLEMENTATION
#define extern static
#endif

/**
 *\brief 创建一个新的链表节点
 *\param[in] data 节点数据
 *\return 新的链表节点指针
 */
extern list_t list_alloc(void *data);

/**
 *\brief 删除整个链表
 *\param[in] list 链表头指针
 *\return 恒为 NULL
 */
extern list_t list_free(list_t list);

/**
 *\brief 删除整个链表
 *\param[in] list 链表头指针
 *\param[in] free_data 释放数据的 callback
 *\return 恒为 NULL
 */
extern list_t list_free_with(list_t list, void (*free_data)(void *));

/**
 *\brief 在链表末尾插入节点
 *\param[in] list 链表头指针
 *\param[in] data 节点数据
 *\return 更新后的链表头指针
 */
extern list_t list_append(list_t list, void *data);

/**
 *\brief 获取链表头
 *\param[in] list 链表指针
 *\return 存在则为指向链表头的指针，否则为 NULL
 */
extern list_t list_head(list_t list);

/**
 *\brief 获取链表尾
 *\param[in] list 链表指针
 *\return 存在则为指向链表尾的指针，否则为 NULL
 */
extern list_t list_tail(list_t list);

/**
 *\brief 获取链表的第 n 项
 *\param[in] list 链表指针 (最好是头指针)
 *\param[in] n 序号 (从 0 开始)
 *\return 存在则为指向该项的指针，否则为 NULL
 */
extern list_t list_nth(list_t list, size_t n);

/**
 *\brief 获取链表的倒数第 n 项
 *\param[in] list 链表指针 (最好是尾指针)
 *\param[in] n 序号 (从 0 开始倒数)
 *\return 存在则为指向该项的指针，否则为 NULL
 */
extern list_t list_nth_last(list_t list, size_t n);

/**
 *\brief 在链表开头插入节点
 *\param[in] list 链表头指针
 *\param[in] data 节点数据
 *\return 更新后的链表头指针
 */
extern list_t list_prepend(list_t list, void *data);

/**
 *\brief 在链表中查找节点
 *\param[in] list 链表头指针
 *\param[in] data 要查找的节点数据
 *\return 若找到对应节点，则返回true，否则返回false
 */
extern bool list_search(list_t list, void *data);

/**
 *\brief 删除链表中的节点
 *\param[in] list 链表头指针
 *\param[in] data 要删除的节点数据
 *\return 更新后的链表头指针
 */
extern list_t list_delete(list_t list, void *data);

/**
 *\brief 删除链表中的节点
 *\param[in] slist 链表头指针
 *\param[in] node 要删除的节点
 *\return 更新后的链表头指针
 */
extern list_t list_delete_node(list_t list, list_t node);

/**
 *\brief 链表的长度
 *\param[in] list 链表头指针
 *\return 链表的长度
 */
extern size_t list_length(list_t list);

/**
 *\brief 打印链表中的节点数据
 *\param[in] list 链表头指针
 */
extern void list_print(list_t list);

#ifdef LIST_IMPLEMENTATION
#undef extern
#endif

#ifdef LIST_IMPLEMENTATION

static list_t list_alloc(void *data) {
  list_t node = (list_t)malloc(sizeof(*node));
  if (node == NULL)
    return NULL;
  node->data = data;
  node->prev = NULL;
  node->next = NULL;
  return node;
}

static list_t list_free(list_t list) {
  while (list != NULL) {
    list_t next = list->next;
    free(list);
    list = next;
  }
  return NULL;
}

static list_t list_free_with(list_t list, void (*free_data)(void *)) {
  while (list != NULL) {
    list_t next = list->next;
    free_data(list->data);
    free(list);
    list = next;
  }
  return NULL;
}

static list_t list_append(list_t list, void *data) {
  list_t node = list_alloc(data);
  if (node == NULL)
    return list;

  if (list == NULL) {
    list = node;
  } else {
    list_t current = list;
    while (current->next != NULL) {
      current = current->next;
    }
    current->next = node;
    node->prev = current;
  }

  return list;
}

static list_t list_prepend(list_t list, void *data) {
  list_t node = list_alloc(data);
  if (node == NULL)
    return list;

  node->next = list;
  if (list != NULL)
    list->prev = node;
  list = node;

  return list;
}

static void *list_pop(list_t *list_p) {
  if (list_p == NULL || *list_p == NULL)
    return NULL;
  auto list = list_tail(*list_p);
  if (*list_p == list)
    *list_p = list->prev;
  if (list->prev)
    list->prev->next = NULL;
  auto data = list->data;
  free(list);
  return data;
}

static list_t list_head(list_t list) {
  if (list == NULL)
    return NULL;
  for (; list->prev; list = list->prev) {
  }
  return list;
}

static list_t list_tail(list_t list) {
  if (list == NULL)
    return NULL;
  for (; list->next; list = list->next) {
  }
  return list;
}

static list_t list_nth(list_t list, size_t n) {
  if (list == NULL)
    return NULL;
  list = list_head(list);
  for (size_t i = 0; i < n; i++) {
    list = list->next;
    if (list == NULL)
      return NULL;
  }
  return list;
}

static list_t list_nth_last(list_t list, size_t n) {
  if (list == NULL)
    return NULL;
  list = list_tail(list);
  for (size_t i = 0; i < n; i++) {
    list = list->prev;
    if (list == NULL)
      return NULL;
  }
  return list;
}

static bool list_search(list_t list, void *data) {
  list_t current = list;
  while (current != NULL) {
    if (current->data == data)
      return true;
    current = current->next;
  }
  return false;
}

static list_t list_delete(list_t list, void *data) {
  if (list == NULL)
    return NULL;

  if (list->data == data) {
    list_t temp = list;
    list = list->next;
    free(temp);
    return list;
  }

  for (list_t current = list->next; current; current = current->next) {
    if (current->data == data) {
      current->prev->next = current->next;
      if (current->next != NULL)
        current->next->prev = current->prev;
      free(current);
      break;
    }
  }

  return list;
}

static list_t list_delete_with(list_t list, void *data,
                               void (*callback)(void *)) {
  if (list == NULL)
    return NULL;

  if (list->data == data) {
    list_t temp = list;
    list = list->next;
    if (callback)
      callback(temp->data);
    free(temp);
    return list;
  }

  for (list_t current = list->next; current; current = current->next) {
    if (current->data == data) {
      current->prev->next = current->next;
      if (current->next != NULL)
        current->next->prev = current->prev;
      if (callback)
        callback(current->data);
      free(current);
      break;
    }
  }

  return list;
}

static list_t list_delete_node(list_t list, list_t node) {
  if (list == NULL || node == NULL)
    return list;

  if (list == node) {
    list_t temp = list;
    list = list->next;
    free(temp);
    return list;
  }

  node->prev->next = node->next;
  if (node->next != NULL)
    node->next->prev = node->prev;
  free(node);
  return list;
}

static list_t list_delete_node_with(list_t list, list_t node,
                                    void (*callback)(void *)) {
  if (list == NULL || node == NULL)
    return list;

  if (list == node) {
    list_t temp = list;
    list = list->next;
    if (callback)
      callback(temp->data);
    free(temp);
    return list;
  }

  node->prev->next = node->next;
  if (node->next != NULL)
    node->next->prev = node->prev;
  if (callback)
    callback(node->data);
  free(node);
  return list;
}

static size_t list_length(list_t list) {
  size_t count = 0;
  list_t current = list;
  while (current != NULL) {
    count++;
    current = current->next;
  }
  return count;
}

#ifdef __libplos__
static void list_print(list_t list) {
  list_t current = list;
  while (current != NULL) {
    printf("%p -> ", current->data);
    current = current->next;
  }
  printf("NULL\n");
}
#endif

#undef LIST_IMPLEMENTATION
#endif

/**
 *\brief 在链表末尾插入节点
 *\param[in,out] list 链表头指针
 *\param[in] data 节点数据
 */
#define list_append(list, data) ((list) = list_append(list, (void *)(data)))

/**
 *\brief 在链表开头插入节点
 *\param[in,out] list 链表头指针
 *\param[in] data 节点数据
 */
#define list_prepend(list, data) ((list) = list_prepend(list, (void *)(data)))

#define list_push(list, data) list_append(list, data)

#define list_pop(list) list_pop(&(list))

#define list_popi(list) ((usize)list_pop(list))
#define list_popu(list) ((isize)list_pop(list))

/**
 *\brief 删除链表中的节点
 *\param[in,out] list 链表头指针
 *\param[in] data 要删除的节点数据
 */
#define list_delete(list, data) ((list) = list_delete(list, data))

#define list_delete_with(list, data, callback)                                 \
  ((list) = list_delete_with(list, data, callback))

/**
 *\brief 删除链表中的节点
 *\param[in,out] list 链表头指针
 *\param[in] node 要删除的节点
 */
#define list_delete_node(slist, node) ((slist) = list_delete_node(slist, node))

#define list_delete_node_with(list, node, callback)                            \
  ((list) = list_delete_node_with(list, node, callback))

/**
 *\brief 遍历链表中的节点并执行操作
 *\param[in] list 链表头指针
 *\param[in] node 用于迭代的节点指针变量
 */
#define list_foreach(list, node)                                               \
  for (list_t node = (list); node; node = node->next)

/**
 *\brief 遍历链表中的节点并执行操作
 *\param[in] list 链表头指针
 *\param[in] node 用于迭代的节点指针变量
 */
#define list_foreach_cnt(list, i, node, code)                                  \
  ({                                                                           \
    size_t i = 0;                                                              \
    for (list_t node = (list); node; (node) = (node)->next, (i)++)             \
      (code);                                                                  \
  })

#define list_first_node(list, node, expr)                                      \
  ({                                                                           \
    list_t _match_ = NULL;                                                     \
    for (list_t node = (list); node; node = node->next) {                      \
      if ((expr)) {                                                            \
        _match_ = node;                                                        \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
    _match_;                                                                   \
  })

#define list_first(list, _data_, expr)                                         \
  ({                                                                           \
    void *_match_ = NULL;                                                      \
    for (list_t node = (list); node; node = node->next) {                      \
      void *_data_ = node->data;                                               \
      if ((expr)) {                                                            \
        _match_ = _data_;                                                      \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
    _match_;                                                                   \
  })
// --------------------------------------
#define PL_READLINE_KEY_UP 0xff00
#define PL_READLINE_KEY_DOWN 0xff01
#define PL_READLINE_KEY_LEFT 0xff02
#define PL_READLINE_KEY_RIGHT 0xff03
#define PL_READLINE_KEY_ENTER '\n'
#define PL_READLINE_KEY_TAB '\t'
#define PL_READLINE_KEY_BACKSPACE '\b'
#define _SELF pl_readline_t self
#define PL_READLINE_SUCCESS 0
#define PL_READLINE_FAILED -1
#define PL_READLINE_NOT_FINISHED 1

typedef struct pl_readline_word {
  char *word; // 词组
  /**
    如果first为true，
    这个word必须在第一个参数的位置的时候才能得到补全
    如abc 则必须输入"ab"然后按tab，才会有可能有"abc"
    如果是“qwe ab”则不会补全"qwe abc"，除非first为false.
  */
  bool first;

  char sep; // 分隔符
} pl_readline_word;

typedef struct pl_readline_words {
  int len;                 // 词组数量
  int max_len;             // 词组最大数量
  pl_readline_word *words; // 词组列表
} * pl_readline_words_t;

typedef struct pl_readline {
  int (*pl_readline_hal_getch)();        // 输入函数
  void (*pl_readline_hal_putch)(int ch); // 输出函数
  void (*pl_readline_hal_flush)();       // 刷新函数
  void (*pl_readline_get_words)(char *buf,
                                pl_readline_words_t words); // 获取词组列表
  list_t history; // 历史记录列表
} * pl_readline_t;
typedef struct pl_readline_runtime {
  char *buffer;           // 输入缓冲区
  int p;                  // 输入缓冲区指针
  int length;             // 输入缓冲区长度（已经输入的字符数）
  int history_idx;        // 历史记录指针
  char *prompt;           // 提示符
  size_t len;             // 缓冲区最大长度
  char *input_buf;        // 输入缓冲区（补全的前缀）
  int input_buf_ptr;      // 输入缓冲区（补全的前缀）指针
  bool intellisense_mode; // 智能补全模式
  char *intellisense_word; // 智能补全词组
} pl_readline_runtime;

pl_readline_words_t pl_readline_word_maker_init();
pl_readline_t pl_readline_init(
    int (*pl_readline_hal_getch)(), void (*pl_readline_hal_putch)(int ch),
    void (*pl_readline_hal_flush)(),
    void (*pl_readline_get_words)(char *buf, pl_readline_words_t words));
int pl_readline(_SELF, char *prompt, char *buffer, size_t len);
pl_readline_word pl_readline_intellisense(_SELF, pl_readline_runtime *rt,
                                          pl_readline_words_t words);
void pl_readline_insert_char_and_view(_SELF, char ch, pl_readline_runtime *rt);
void pl_readline_insert_char(char *str, char ch, int idx);
int pl_readline_word_maker_add(char *word, pl_readline_words_t words,
                               bool is_first, char sep);
void pl_readline_print(_SELF, char *str);
void pl_readline_intellisense_insert(_SELF, pl_readline_runtime *rt,
                                     pl_readline_word words);
void pl_readline_word_maker_destroy(pl_readline_words_t words);
void pl_readline_next_line(_SELF, pl_readline_runtime *rt);
int pl_readline_handle_key(_SELF, int ch, pl_readline_runtime *rt);
void pl_readline_uninit(_SELF);