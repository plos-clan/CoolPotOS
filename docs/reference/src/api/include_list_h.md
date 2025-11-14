# include/list.h

## `typedef struct list *list_t;`


\struct ListNode
\brief 链表节点结构


---

## `extern list_t list_alloc(void *data);`


\brief 创建一个新的链表节点
\param[in] data 节点数据
\return 新的链表节点指针


---

## `extern list_t list_free(list_t list);`


\brief 删除整个链表
\param[in] list 链表头指针
\return 恒为 NULL


---

## `extern list_t list_free_with(list_t list, free_t free_data);`


\brief 删除整个链表
\param[in] list 链表头指针
\param[in] free_data 释放数据的 callback
\return 恒为 NULL


---

## `extern list_t list_append(list_t list, void *data);`


\brief 在链表末尾插入节点
\param[in] list 链表头指针
\param[in] data 节点数据
\return 更新后的链表头指针


---

## `extern list_t list_head(list_t list);`


\brief 获取链表头
\param[in] list 链表指针
\return 存在则为指向链表头的指针，否则为 NULL


---

## `extern list_t list_tail(list_t list);`


\brief 获取链表尾
\param[in] list 链表指针
\return 存在则为指向链表尾的指针，否则为 NULL


---

## `extern list_t list_nth(list_t list, size_t n);`


\brief 获取链表的第 n 项
\param[in] list 链表指针 (最好是头指针)
\param[in] n 序号 (从 0 开始)
\return 存在则为指向该项的指针，否则为 NULL


---

## `extern list_t list_nth_last(list_t list, size_t n);`


\brief 获取链表的倒数第 n 项
\param[in] list 链表指针 (最好是尾指针)
\param[in] n 序号 (从 0 开始倒数)
\return 存在则为指向该项的指针，否则为 NULL


---

## `extern list_t list_prepend(list_t list, void *data);`


\brief 在链表开头插入节点
\param[in] list 链表头指针
\param[in] data 节点数据
\return 更新后的链表头指针


---

## `extern bool list_search(list_t list, void *data);`


\brief 在链表中查找节点
\param[in] list 链表头指针
\param[in] data 要查找的节点数据
\return 若找到对应节点，则返回true，否则返回false


---

## `extern list_t list_delete(list_t list, void *data);`


\brief 删除链表中的节点
\param[in] list 链表头指针
\param[in] data 要删除的节点数据
\return 更新后的链表头指针


---

## `extern list_t list_delete_node(list_t list, list_t node);`


\brief 删除链表中的节点
\param[in] slist 链表头指针
\param[in] node 要删除的节点
\return 更新后的链表头指针


---

## `extern size_t list_length(list_t list);`


\brief 链表的长度
\param[in] list 链表头指针
\return 链表的长度


---

## `extern void list_print(list_t list);`


\brief 打印链表中的节点数据
\param[in] list 链表头指针


---

## `#define list_foreach(list, node) for (list_t node = (list);`


\brief 遍历链表中的节点并执行操作
\param[in] list 链表头指针
\param[in] node 用于迭代的节点指针变量


---

## `#define list_foreach_cnt(list, i, node, code) \ ({`


\brief 遍历链表中的节点并执行操作
\param[in] list 链表头指针
\param[in] node 用于迭代的节点指针变量


---

