# include/mem/page.h

## `uint64_t page_alloc_random(page_directory_t *directory, uint64_t length, uint64_t flags);`


分配一块随机的内核区可用地址

- **`directory`**: 源页表
- **`length`**: 分配长度
- **`flags`**: 分配标志


---

## `void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length, uint64_t flags);`


映射一段物理地址不连续的区域

- **`directory`**: 源页表
- **`addr`**: 虚拟地址起始
- **`length`**: 映射长度
- **`flags`**: 映射标志


---

## `void unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size);`


解除一段地址映射
注意: 该函数会连带释放掉这段地址映射所被分配的物理页框占用

- **`directory`**: 源页表
- **`vaddr`**: 虚拟地址起始
- **`size`**: 大小


---

## `page_directory_t *get_current_directory();`


获取当前页表
注意: 必须在 smp 初始化后使用

- **Returns**: 当前页表 (为NULL则smp未初始化)


---

## `page_directory_t *switch_context_directory(page_directory_t *directory);`


切换当前页表 (当前进程的页表也会被切换)

- **`directory`**: 源页表
- **Returns**: 被换下来的页表


---

## `uint64_t arch_virt_to_phys(uint64_t va);`


根据页表反向解析出物理地址

- **`va`**: 虚拟地址
- **Returns**: 物理地址


---

