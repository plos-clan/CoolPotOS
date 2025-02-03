# CoolPotOS for x86

## 翻译

- [English](/README.md)
- **简体中文**
- [Français](/readme/README-fr-FR.md)

## 介绍

这是一个基于 x86 架构的简单操作系统。

## 模块

* `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
* `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)

## 构建并运行

### 环境

您需要安装这些软件：

- Xmake
- NASM
- Zig（如果 xmake 无法为您下载它，您可以手动安装）
- Windows subsystem for Linux (Ubuntu 22.04)
    - xorriso
    - qemu-system-i386

### 步骤

- 在终端上运行 `xmake run`，项目将开始构建并运行

## 许可协议

该项目完全遵循 MIT 协议，任何人都可以免费使用它，另见 [LICENSE](LICENSE)。

## 贡献

欢迎为这个项目提交 PR 或 issue，`然后坐和放宽`

### 贡献者们

* XIAOYI12 - 负责主要的OS开发
* min0911Y - 负责OS文件系统开发
* copi143 - 新版用户堆框架开发
* QtLittleXu - 负责OS文档编写
* ViudiraTech - PCI驱动优化
* VinbeWan - IIC驱动程序开发
* A4-Tacks - 编写一些构建脚本
* wenxuanjun - OS 重构开发
* Minsecrus - OS 内存统计算法优化
* CLimber-Rong - 软件开发
* shiyu - 负责到处捉虫和帮助代码添加注释
* 27Onion - 翻译了法语README
* LY-Xiang - 优化了 actions 流程
