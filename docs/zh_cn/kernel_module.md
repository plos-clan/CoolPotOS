# 内核模块

## 简介

内核模块是操作系统内核一部分, 运行在与内核相同的地址空间与特权级中, 具有完全的内核访问权限. \
`CP_Kernel` 的内核模块为 `elf` 格式的动态链接库, 后缀名为 `*.km`.

> 内核模块一旦被装载进内核, 就无法被卸载, 只能在下次系统重启时生效.

目前已有内核模块:

- `extfs` - ext 文件系统实现

## 源码目录

位于项目的 `module` 目录下, 包含内核模块的源码开发头文件与各个内核模块的源码.

* all_include/cp_kernel.h: 内核模块开发的公共头文件, 包含基础库函数与类型封装等.
* all_include/fs_subsystem.h: 文件系统子系统的接口导出.
* all_include/mem_subsystem.h: 内存管理子系统的接口导出.
* all_include/int_subsystem.h: 中断与时钟子系统的接口导出.
* all_include/driver_subsystem.h: 设备管理子系统的接口导出.
* all_include/errno.h: 错误码定义.
* all_include/lock.h: 锁类型与接口定义.

## 示例

`extfs` 内核模块的源码位于 `module/extfs` 目录下, 其他内核模块可参照该项目为模板在 `module` 目录下创建新的内核模块.
