# include/mem/alloc/freelist.h

## `static inline freelist_t freelist_detach(freelist_t list, freelist_t ptr) {`


\brief 将 freelist 中的内存块分离

\param list     空闲链表
\param ptr      要分离的内存块指针
\return 分离后的空闲链表


---

## `static inline void *freelists_detach(freelists_t lists, int id, freelist_t ptr) {`


\brief 将 freelist 中的内存块分离

\param lists    空闲链表组
\param id       空闲链表的 id
\param ptr      要分离的内存块指针
\return 分离的内存块指针 (即传入的 ptr)


---

## `static inline void *freelist_match(freelist_t *list_p, size_t size) {`


\brief 匹配并将内存从 freelist 中分离

\param list_p   空闲链表指针
\param size     要寻找内存的最小大小
\return 找到的内存块指针，未找到为 NULL


---

## `static inline void *freelists_match(freelists_t lists, size_t size) {`


\brief 匹配并将内存从 freelist 中分离

\param lists    空闲链表 (组)
\param size     要寻找内存的最小大小
\return 找到的内存块指针，未找到为 NULL


---

## `static inline void *freelist_aligned_match(freelist_t *list_p, size_t size, size_t align) {`


\brief 匹配并将内存从 freelist 中分离
要求内存能对齐到 align 指定的大小且对齐后至少有 size 的大小

\param list_p   空闲链表指针
\param size     要寻找内存的最小大小
\param align    对齐大小 (必须为 2 的幂) (必须大于等于 2 倍字长)
\return 找到的内存块指针，未找到为 NULL


---

## `static inline void *freelists_aligned_match(freelists_t lists, size_t size, size_t align) {`


\brief 匹配并将内存从 freelist 中分离
要求内存能对齐到 align 指定的大小且对齐后至少有 size 的大小

\param lists    空闲链表 (组)
\param size     要寻找内存的最小大小
\param align    对齐大小 (必须为 2 的幂) (必须大于等于 2 倍字长)
\return 找到的内存块指针，未找到为 NULL


---

## `static inline void freelist_put(freelist_t *list_p, freelist_t ptr) {`


\brief 将元素放到 freelist 中

\param list_p   空闲链表指针
\param ptr      内存块指针


---

## `static inline bool freelists_put(freelists_t lists, void *_ptr) {`


\brief 将元素放到 freelist 中

函数若失败应将内存块放到大内存块空闲链表中

\param lists    空闲链表 (组)
\param ptr      内存块指针
\return 是否成功


---

