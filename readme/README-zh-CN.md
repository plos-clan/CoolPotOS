<div align="center">
<img height="200px" src="https://github.com/user-attachments/assets/9542ad95-0f48-43ad-9617-a750db84e907" />

<h1 align="center">CoolPotOS</h1>
<h3>一个简单的玩具操作系统</h3>

<img alt="GitHub License" src="https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub release (latest by date)" src="https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub issues" src="https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="Hardware" src="https://img.shields.io/badge/Hardware-i386_x64-blue?style=flat-square"/>
</div>

---

## 翻译

- [English](/README.md)
- *简体中文*
- [Français](/readme/README-fr-FR.md)
- [日本語](/readme/README-ja-JP.md)

## 介绍

这是一个在 [ia32](https://en.wikipedia.org/wiki/IA-32) 或 [amd64](https://en.wikipedia.org/wiki/X86-64) 架构上运行的简单操作系统。

## 模块

- `pl_readline` 来自 min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` 来自 wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
- `plant-vfs` 来自 min0911Y [plos-clan/plant-vfs](https://github.com/plos-clan/plant-vfs)
- `musllibc` 来自 seL4 [seL4/musllibc](https://github.com/seL4/musllibc)

## 构建并运行

### 环境

您需要安装这些软件：

- xmake
- xorriso
- QEMU
- NASM (仅i386需要)
- Zig (仅i386需要, 由 xmake 自动安装)
- git (仅x86_64需要, 用于生成 `GIT_VERSION` 宏)
- clang (仅x86_64需要)
- lld (仅x86_64需要, 用于链接 LTO 对象)
- Rust nightly toolchain (仅x86_64需要)
- cbindgen (仅x86_64需要, 使用 `cargo install cbindgen` 安装)
- oib (仅x86_64构建IMG镜像需要, 使用 `cargo install oib` 安装)

### 选项

你可以使用以下命令设置目标架构（默认 `x86_64`）：

```bash
xmake f -y --arch=i686
```

### 可用命令

- `xmake run` - 构建并运行 **ISO** 镜像
- `xmake build iso` - 构建 ISO 镜像，不运行
- `xmake build img` - 构建 IMG 磁盘镜像，不运行

## 开发

你可以使用以下命令生成 `compile_commands.json` 文件：

```bash
xmake project -k compile_commands
```

这样你的编辑器就能够启用语法高亮与跳转等功能。

## 许可协议

该项目完全遵循 MIT 协议，任何人都可以免费使用它，另见 [LICENSE](/LICENSE)。

## 支持功能

### AMD64

基于 UEFI BIOS 引导. \
使用 Limine 引导器.

- [x] 4级页表内存管理
- [x] xapic 与 x2apic 高级可编程中断控制器支持
- [x] 内核模块支持
- [x] AHCI 硬盘设备驱动
- [x] 多任务支持 (进程与线程)
- [x] PS/2 键盘和鼠标驱动支持
- [x] PCIE 设备枚举
- [x] ACPI 电源管理
- [x] VFS VDisk 抽象层接口
- [x] 进程消息队列
- [ ] 进程信号机制
- [x] 适用于多核CPU的调度器
- [x] 用户态应用程序
- [x] 设备文件系统
- [x] 浮点协处理器
- [ ] IIC 总线驱动
- [ ] Nvme 和 USB 驱动
- [ ] PCNET 和 Rtl8169 驱动
- [x] SB16 和 PCSpeaker 驱动
- [ ] TTY 驱动
- [x] SATA/SATAPI 驱动

## 贡献

欢迎为这个项目提交 PR 或 issue，`然后坐和放宽`

### 贡献者们

* 前往 [CoolPotOS | Website](cpos.plos-clan.org) 查看贡献者列表
