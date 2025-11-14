# all_include/fs_subsystem.h

## `errno_t vfs_mkdir(const char *name);`


创建目录节点

- **`name`**: 绝对路径
- **Returns**: 非0代表创建失败


---

## `errno_t vfs_mkfile(const char *name);`


创建文件节点

- **`name`**: 绝对路径
- **Returns**: 非0代表创建失败


---

## `int vfs_regist(const char *name, vfs_callback_t callback, uint64_t magic);`


注册文件系统回调指针
- 注意: 所有的回调函数都必须实现不得为NULL, 否则会注册失败.
(如果不需要某个回调函数, 用一个空实现替代即可)

- **`name`**: 文件系统名
- **`callback`**: 回调指针
- **`register_id`**: 文件系统挂载id (非虚拟文件系统填0)
- **`magic`**: 文件系统属性类型
- **Returns**: 文件系统id


---

## `errno_t vfs_link(const char *name, const char *target_name);`


创建 link 文件

- **`name`**: 被链接文件
- **`target_name`**: 创建的link文件名
- **Returns**: 是否成功创建


---

## `errno_t vfs_symlink(const char *name, const char *target_name);`


创建一个符号链接文件

- **`name`**: 被链接文件
- **`target_name`**: 创建的链接文件
- **Returns**: 是否成功创建


---

## `vfs_node_t vfs_child_append(vfs_node_t parent, const char *name, void *handle);`


向父节点添加一个子节点

- **`parent`**: 父节点
- **`name`**: 子节点名称
- **`handle`**: 节点句柄
- **Returns**: 子节点


---

## `errno_t vfs_close(vfs_node_t node);`


关闭一个已打开的 vfs 节点

- **`node`**: 
- **Returns**: 非0 代表错误返回


---

## `void vfs_free(vfs_node_t vfs);`


释放一个 vfs 节点

- **`vfs`**: 节点


---

## `void vfs_update(vfs_node_t node);`


更新 vfs 节点的状态

- **`node`**: 节点


---

## `vfs_node_t vfs_open(const char *str);`


打开一个节点

- **`str`**: 路径(绝对路径)
- **Returns**: 为NULL代表打开失败


---

## `errno_t vfs_ioctl(vfs_node_t device, size_t options, void *arg);`


向一个节点发送 I/O 控制命令

- **`device`**: 节点设备
- **`options`**: 操作码
- **`arg`**: 参数
- **Returns**: 非0代表操作失败


---

## `size_t vfs_readlink(vfs_node_t node, char *buf, size_t bufsize);`


读取一个符号链接文件

- **`node`**: 文件节点
- **`buf`**: 缓冲区
- **`bufsize`**: 
- **Returns**: 


---

## `vfs_filesystem_t get_filesystem(char *type);`


根据类型获取文件系统句柄

- **`type`**: 类型
- **Returns**: 句柄(NULL为找不到)


---

## `vfs_filesystem_t get_filesystem_node(vfs_node_t node);`


获取指定文件节点的文件系统描述符

- **`node`**: 文件节点
- **Returns**: 文件系统描述符


---

## `errno_t vfs_mount(const char *src,const char *type, vfs_node_t node);`


挂载一个文件系统到指定节点
挂载后，src 代表设备的路径，node 代表挂载点
挂载点必须是一个目录


- **`src`**: 设备路径
- **`node`**: 挂载点
- **`type`**: 文件系统类型
- **Returns**: 非0代表挂载失败


---

## `errno_t vfs_unmount(const char *path);`


卸载一个挂载点
- 注意: 一些内核常驻挂载点不会被此函数卸载, 如 devfs/tmpfs 等

- **`path`**: 挂载点路径
- **Returns**: 非0代表写在失败


---

## `vfs_node_t get_rootdir();`


获取根目录节点

- **Returns**: 根目录节点


---

