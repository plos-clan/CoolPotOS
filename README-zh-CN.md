# CoolPotOS for x86

## 翻译

- [English](README.md)
- [中文](README-zh-CN.md)

## 介绍

这是一个适用于 x86_64 架构的简单操作系统

## 模块

* `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
* `Uinxed-Mark` by ViudiraTech [ViudiraTech/Uinxed-Kernel](https://github.com/ViudiraTech/Uinxed-Kernel)
* `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
* `libelf_parser` by wenxuanjun [plos-clan/libelf_parser](https://github.com/plos-clan/libelf-parse)

## 构建并运行

### 环境

您需要安装这些软件：

- Xmake
- NASM
- Clang（如果 xmake 无法为您下载它，您可以手动安装）
- Windows subsystem for Linux (Ubuntu 22.04)
    - xorriso
    - qemu-system-i386

### 步骤

- 在终端上运行 `xmake run`，项目将开始构建并运行

## 许可协议

该项目完全遵循 MIT 协议，任何人都可以免费使用它，另见 [LICENSE](LICENSE)。

## 贡献

欢迎为这个项目提交 PR 或 issue，我会非常高兴看到它！

### 贡献者们

* XIAOYI12 - OS 主要开发者
* wenxuanjun - OS 主要开发者
