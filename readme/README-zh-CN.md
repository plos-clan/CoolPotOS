<div align="center">
<img height="200px" src="https://github.com/user-attachments/assets/9542ad95-0f48-43ad-9617-a750db84e907" />

<h1 align="center">CoolPotOS</h1>
<h3>一个简单的玩具操作系统</h3>

![GitHub Repo stars](https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square)
![GitHub issues](https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square)
![GitHub License](https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square)
![GitHub release (latest by date)](https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square)
![Hardware](https://img.shields.io/badge/Hardware-i386_x64-blue?style=flat-square)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/plos-clan/CoolPotOS)
</div>

---

Languages
: [English](../README.md)
| *简体中文*
| [Français](README-fr-FR.md)
| [日本語](/readme/README-ja-JP.md)

## 介绍

这是一个在 [ia32](https://en.wikipedia.org/wiki/IA-32)
或 [amd64](https://en.wikipedia.org/wiki/X86-64) 架构上运行的简单操作系统。

## 模块

- `pl_readline` 来自 min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` 来自 wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
- `plant-vfs` 来自 min0911Y [plos-clan/plant-vfs](https://github.com/plos-clan/plant-vfs)
- `EEVDF` 来自 xiaoyi1212 [plos-clan/EEVDF](https://github.com/plos-clan/EEVDF)
- `libfdt` 来自 osdev [osdev/libfdt](https://codeberg.org/OSDev/libfdt)

## 构建与运行

你需要下载以下工具才能编译 CoolPotOS

- cmake
- xorriso
- QEMU
- git (`GIT_VERSION` 宏哈希获取)
- clang 
- lld
- openssl (内核签名密钥生成)
- python3 `cryptography` (签名内核模块)
- 
### 参数

你可以在命令行指定需要编译的 CoolPotOS 架构 (默认为 `x86_64`):

```bash
cmake -S . -B build/ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=<mode> -DTARGET_ARCH=<arch>
cmake --build build/ --target run
```

* mode - `Release` | `Debug`
* arch - `x86_64` | `riscv64` | `aarch64`

然后复制 `build/compile_commands.json` 到你的项目根目录即可.

## 许可协议

该项目完全遵循 MIT 协议，任何人都可以免费使用它，另见 [LICENSE](/LICENSE)。

## 贡献

欢迎为这个项目提交 PR 或 issue，`然后坐和放宽`

### 贡献者们

* 前往 [CoolPotOS | Website](https://cpos.plos-clan.org) 查看贡献者列表
