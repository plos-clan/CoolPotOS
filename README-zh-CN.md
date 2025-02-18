
<br>
<br>

<h1 align="center">CoolPotOS</h1>

<div align="center"><strong>一个简单的玩具操作系统.</strong></div>

<br>
<br>

<div align="center">
<img alt="GitHub License" src="https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub release (latest by date)" src="https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub issues" src="https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="Hardware" src="https://img.shields.io/badge/Hardware-i386_x64-blue?style=flat-square"/>
</div>



<hr>

## 翻译

- [English](README.md)
- [中文](README-zh-CN.md)

## 介绍

这是一个适用于 x86_64 架构的简单操作系统

## 模块

* `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
* `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)

## 构建并运行

### 环境

您需要安装这些软件：

- Xmake
- NASM (如果您编译的是x86_64分支则无需下载)
- Clang（如果 xmake 无法为您下载它，您可以手动安装）
- Windows subsystem for Linux (Ubuntu 22.04)
    - xorriso
    - qemu-system-x86_64

### 步骤

- 在终端上运行 `xmake run`，项目将开始构建并运行

## 许可协议

该项目完全遵循 MIT 协议，任何人都可以免费使用它，另见 [LICENSE](LICENSE)。

## 贡献

欢迎为这个项目提交 PR 或 issue，我会非常高兴看到它！

### 贡献者们

* XIAOYI12 - 负责主要的OS开发
* min0911Y - 负责OS文件系统开发
* copi143 - 新版用户堆框架开发
* QtLittleXu - 负责OS文档编写
* ViudiraTech - PCI驱动优化
* VinbeWan - IIC驱动程序开发
* A4-Tacks - 编写一些构建脚本
* wenxuanjun - OS 内存管理开发
* Minsecrus - OS 内存统计算法优化
* CLimber-Rong - 软件开发
