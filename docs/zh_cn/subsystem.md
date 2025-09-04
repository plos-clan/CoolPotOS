# CP_Kernel 子系统

## 简介

目前CPOS内核拥有以下子系统

- 多任务管理子系统
- 内存管理子系统
- 中断与时钟子系统
- 设备管理子系统
- 文件系统子系统
- 服务与模块 (非子系统, 属功能组件)

## Process Subsystem (多任务管理子系统)

其源码位于 `task` 目录下, 负责进程/线程创建, 挂起, 销毁和进程信号与通信, 也包含调度器的实现.

- eevdf.c: 最小虚拟截止时间优先调度算法的核心实现部分.
- futex.c: 线程的挂起/唤醒机制实现.
- ipc.c: 进程间通信机制实现.
- prsys.c: 多任务有关的系统调用实现, 如 fork, exec, clone 等.
- signal.c: 信号机制实现. (TODO: 尚未完成)
- scheduler.c: 调度器通用封装部分, 与具体算法实现解耦.
- poll.c: I/O 多路复用机制实现.

## Memory Subsystem (内存管理子系统)

其源码位于 `mem` 目录下, 包含物理页框分配管理, 内存统计, 内核堆分配器实现, 懒分配器实现.

- heap/: 内核堆分配器实现, 由 `copi143` 编写的 `plalloc` 分配器.
- lazyalloc.c: 懒分配器实现.
- frame.c: 物理页框分配器实现.
- memstats.c: 内存统计实现.
- page.c: 页表管理相关实现.
- bitmap.c: 位图实现, 页框分配器使用位图来管理物理内存.
- hhdm.c: 有关于高半部映射的内存偏移获取以及一些虚实地址转换.

## Interrupt and Clock Subsystem (中断与时钟子系统)

其源码同时存在于 `cpu` 和 `driver` 目录下, 并不是一个独立的子系统.

- cpu/error_handle.c: 内核异常处理实现.
- cpu/idt.c: 中断描述符表初始化与中断处理程序的注册.
- cpu/empty_interrupt.c: 无中断处理程序调试检查.
- cpu/cpustats.c: 高精度实时计数器实现(提供调度器一个实时的时钟源).
- driver/acpi/hpet.c: 高精度事件定时器(HPET)驱动, 校准高精度实时计数器与 `local apic` 定时器.
- driver/acpi/apic.c: 高级可编程中断控制器的初始化(包含 `local apic` 与 `I/O apic` 的初始化).

## Device Subsystem (设备管理子系统)

其源码位于 `driver` 目录下, 负责设备的注册与管理, 包含常见的设备驱动实现.

- /fs/devfs.c: 设备文件系统实现, 是设备列表的映射, 也提供给用户态和内核本身一些设备操作的接口.
- pci/: PCI总线与其关联的设备驱动实现.
- acpi/: 高级可配置与电源接口(ACPI)相关的驱动实现, 包含电源管理.
- iic/: I2C总线与其关联的设备驱动实现(TODO: 尚未测试).
- input/: 输入设备驱动实现, 包含键盘与鼠标驱动.
- sound/: 内置声卡驱动实现, 目前仅包含 `PC Speaker` 和 `sb16` 驱动.
- dma.c: DMA控制器驱动实现.
- rtc.c: CMOS 芯片驱动实现, 提供实时时间功能.
- gop.c: 基础的图形输出协议驱动实现, 提供简单的图形输出功能.
- serial.c: 串口驱动实现, 提供串口通信功能.
- tty.c: 终端设备驱动实现, 提供字符终端功能.
- urandom.c: 随机数设备驱动实现, 提供伪随机数生成功能.
- zero.c: 零设备与null设备驱动实现, 提供读零写丢弃功能.
- vdisk.c: 设备管理器实现, 其操作接口与设备信息被 `devfs` 表现.

## File System Subsystem (文件系统子系统)

其源码位于 `fs` 目录下, 负责文件系统的注册与管理, 并内置了 `fatfs` `iso9660` 文件系统的实现.

- vfs.c: 虚拟文件系统管理, 提供文件系统的注册与挂载, 以及封装文件操作的接口.
- fatfs/: `FAT` 文件系统实现.
- devfs.c: 设备文件系统实现, 属于设备管理子系统的一部分.
- iso9660.c: `ISO9660` 文件系统实现.
- tmpfs.c: 临时文件系统实现, 提供一个基于内存的文件系统.
- pipe.c: 管道实现, 提供进程间通信的管道机制.
- modfs.c: 模块文件系统实现, 提供内核模块的加载与管理功能.
- partition.c: 分区管理实现, 提供对磁盘分区的识别与操作功能, 并将识别到的分区作为虚拟块设备挂载到 `devfs`.

## Services and Modules (服务与模块)

其源码位于 `service` 和 `kmod` 目录下, 并不是一个独立的子系统, 属于内核的功能组件.

- service/killer.c: 负责回收僵尸进程的内核服务.
- service/syscall.c: 提供系统调用接口的内核服务.
- service/sysuser.c: 提供会话与权限管理的内核服务.
- kmod/dlinker.c: 动态链接器实现, 提供对内核模块的动态加载与符号解析功能.
- kmod/module.c: 对引导器加载的内核模块进行封装, 并将文件挂载到 `modfs`.
- kmod/interface.c: 提供内核模块与内核本身交互的接口.
