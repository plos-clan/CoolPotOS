# arch/x86_64/include/page_x64.h

## `page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy);`


克隆一个新的页表

- **`dir`**: 源页表
- **`all_copy`**: 是否拷贝内核部分内存映射
- **Returns**: 新页表 (需要释放)


---

