<div align="center">
<img height="200px" src="https://github.com/user-attachments/assets/9542ad95-0f48-43ad-9617-a750db84e907" />

<h1 align="center">CoolPotOS</h1>
<h3>A simple toy operating system.</h3>

![GitHub Repo stars](https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square)
![GitHub issues](https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square)
![GitHub License](https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square)
![GitHub release (latest by date)](https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square)
![Hardware](https://img.shields.io/badge/Hardware-i386_x64-blue?style=flat-square)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/plos-clan/CoolPotOS)
</div>

---

Languages
: *English*
| [简体中文](readme/README-zh-CN.md)
| [Français](readme/README-fr-FR.md)
| [日本語](readme/README-ja-JP.md)

## Introduction

This is a simple operating system for [ia32](https://en.wikipedia.org/wiki/IA-32)
and [amd64](https://en.wikipedia.org/wiki/X86-64) architecture.

## Modules

- `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
- `plant-vfs` by min0911Y [plos-clan/plant-vfs](https://github.com/plos-clan/plant-vfs)
- `EEVDF` by xiaoyi1212 [plos-clan/EEVDF](https://github.com/plos-clan/EEVDF)

## Build & Run

You need to install them on your computer:

- cmake
- xorriso
- QEMU
- git (x86_64 only, for `GIT_VERSION` macro)
- clang (x86_64 only)
- lld (x86_64 only, for linking LTO objects)

### Options

You can use the command to set the target architecture (`x86_64` default):

```bash
cmake -S . -B build/ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/ --target run
```

Copy `build/compile_commands.json` to the project root directory.

## License

This project is licensed under [MIT License](LICENSE).

## Feature

Based on UEFI BIOS boot. \
Use Limine bootloader.

### AMD64

### RISCV64

### LOONGARCH

### AARCH64

## Contributing

Welcome to create pull requests or issues to this project. Then sit back and relax.

### Contributors

* Goto [CoolPotOS | Website](cpos.plos-clan.org) to see the contributors list.