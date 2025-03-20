# CoolPotOS for x86

## 翻译

- [English](/README.md)
- **简体中文**
- [Français](/readme/README-fr-FR.md)
- [日本語](/readme/README-ja-JP.md)

## 介绍

这是一个在 [ia32](https://en.wikipedia.org/wiki/IA-32) 或 [amd64](https://en.wikipedia.org/wiki/X86-64) 架构上运行的简单操作系统。

## 模块

- `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)

## 构建并运行

### 环境

您需要安装这些软件：

- Xmake
- NASM (x86_64架构无需此汇编器)
- Zig (x86_64架构需要clang编译器)
- Windows subsystem for Linux (Ubuntu 22.04)
  - xorriso
  - qemu-system-i386 / qemu-system-x86_64

### 步骤

#### i386

在 `xmake.lua` 的默认构建目标 `default_build` 中，注释掉 `add_deps("iso64")` 并取消注释 `add_deps("iso32")`。
同时，注释掉 `xmake run x86_64` 下的运行参数，并取消注释 `xmake run i386` 下的运行参数。

在终端上运行 `xmake run`，项目将开始构建并运行

#### x86_64

在终端上运行 `xmake run`，项目将开始构建并运行

## 许可协议

该项目完全遵循 MIT 协议，任何人都可以免费使用它，另见 [LICENSE](/LICENSE)。

## 贡献

欢迎为这个项目提交 PR 或 issue，`然后坐和放宽`

### 贡献者们

- XIAOYI12 - 负责主要的OS开发
- min0911Y - 负责OS文件系统开发
- copi143 - 新版用户堆框架开发
- QtLittleXu - 负责OS文档编写
- ViudiraTech - PCI驱动优化
- VinbeWan - IIC驱动程序开发
- A4-Tacks - 编写一些构建脚本
- wenxuanjun - OS 重构开发
- Minsecrus - OS 内存统计算法优化
- CLimber-Rong - 软件开发
- shiyu - 负责到处捉虫和帮助代码添加注释
- 27Onion - 翻译了法语README
- LY-Xiang - 优化了 actions 流程
- suhuajun-github - 修复AHCI驱动的BUG
- FengHeting - SMBIOS 驱动开发
