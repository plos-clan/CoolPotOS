<div style="height: 3em;"></div>

<h1 align="center">CoolPotOS</h1>

<div align="center"><strong>A simple toy operating system.</strong></div>

<div style="height: 3em;"></div>

<div align="center">
<img alt="GitHub License" src="https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub release (latest by date)" src="https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub issues" src="https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="Hardware" src="https://img.shields.io/badge/Hardware-i386_x64-blue?style=flat-square"/>
</div>

<hr>

Languages
: *English*
| [简体中文](readme/README-zh-CN.md)
| [Français](readme/README-fr-FR.md)
| [日本語](readme/README-ja-JP.md)

## Introduction

This is a simple operating system for [ia32](https://en.wikipedia.org/wiki/IA-32) and [amd64](https://en.wikipedia.org/wiki/X86-64) architecture.

## Modules

- `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
- `liballoc` by wenxuanjun [plos-clan/liballoc](https://github.com/plos-clan/liballoc)

## Build & Run

### Environment

You need to install them on your computer:

- Xmake
- NASM (only i386)
- Zig (you need clang for x86_64)
- Windows subsystem for Linux (Ubuntu 22.04)
  - xorriso
  - qemu-system-i386 / qemu-system-x86_64

### Steps

- Run `xmake run` on your terminal then it will build and run automatically

## License

This project is licensed under [MIT License](LICENSE).

## Contributing

Welcome to create pull requests or issues to this project. Then sit back and relax.

### Contributors

- XIAOYI12 - OS Development
- min0911Y - OS FileSystem Development
- copi143 - UserHeap Development
- QtLittleXu - XuYuxuan OS Document
- ViudiraTech - PCI Driver Optimization
- Vinbe Wan - IIC Development
- A4-Tacks - Write Some Build Scripts
- wenxuanjun - OS Developer
- Minsecrus - Memory Usage Optimization
- CLimber-Rong - Software Developer
- shiyu - Debugger and Comments Writer
- 27Onion - Translated French README
- LY-Xiang - Optimized actions process
- suhuajun-github - Fix several bugs in the AHCI driver
- FengHeting - SMBIOS driver Development
