# CoolPotOS for x86

## Translations

- [English](README.md)
- [中文](README-zh-CN.md)

## Introduction

This is a simple operating system for x86 architecture.

## Model

* `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
* `Uinxed-Mark` by ViudiraTech [ViudiraTech/Uinxed-Kernel](https://github.com/ViudiraTech/Uinxed-Kernel)
* `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
* `libelf_parser` by wenxuanjun [plos-clan/libelf_parser](https://github.com/plos-clan/libelf-parse)

## Build & Run

### Environment

You need to install them on your computer:

- Xmake
- NASM
- Zig (you can install manually if xmake cannot download it for you)
- Windows subsystem for Linux (Ubuntu 22.04)
    - xorriso
    - qemu-system-i386

### Steps

- Run `xmake run` on your terminal then it will build and run automatically

## License

The project follows MIT license. Anyone can use it for free. See [LICENSE](LICENSE).

## Contributing

Welcome to create pull requests or issues to this project. I am really happy to see it!

### Contributors

* XIAOYI12 - xiaoyi12 OS Development
* min0911Y - min0911 OS FileSystem Development
* copi143 - copi143 UserHeap Development
* QtLittleXu - XuYuxuan OS Document
* ViudiraTech - Uinxed-Mark program
* Vinbe Wan - CoolPot_Driver Development
* A4-Tacks - Write some build scripts
* wenxuanjun - OS Developer
* Minsecrus - OS Developer
