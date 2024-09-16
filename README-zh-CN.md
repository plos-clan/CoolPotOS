# CoolPotOS for x86

## 翻译

- [English](README.md)
- [中文](README-zh-CN.md)

## 介绍

这是一个基于 x86 架构的简单操作系统。

## 模块

* `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
* `Uinxed-Mark` by ViudiraTech [ViudiraTech/Uinxed-Kernel](https://github.com/ViudiraTech/Uinxed-Kernel)


## 构建并运行

### 环境

您需要安装这些软件：

- Python 工具包
- i686_elf_tools
- 适用于Windows的Linux子系统 (Ubuntu 22.04)
  - grub-mkrescue
  - qemu-system-i386

### 步骤

- 在终端上运行 `python build.py`，项目将开始构建并运行

## 许可协议

该项目完全遵循 MIT 协议，任何人都可以免费使用它，另见 [LICENSE](LICENSE)。

## 贡献

欢迎为这个项目提交 PR 或 issue，我会非常高兴看到它！

### 贡献者们

* XIAOYI12 - xiaoyi1212 负责主要的OS开发
* min0911Y - min0911 负责OS文件系统开发
* copi143 - copi143 新版用户堆框架开发
* QtLittleXu - XuYuxuan 负责OS文档编写
* ViudiraTech - Uinxed-Mark 性能测试程序
* * Vinbe Wan - IIC 驱动程序开发
