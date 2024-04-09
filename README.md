# CrashPowerDOS for x86

## Translations

- [English](README.md)
- [中文](README-zh-CN.md)

## Introduction

This is a simple operating system for x86 architecture.

## Build & Run

### Environment

You need to install them on your computer:

- Python toolkit
- i686_elf_tools

### Steps

- Run `python build.py` on your terminal then it is going to build
- Run `grub-mkrescue -o cpos.iso isodir` to package the iso file
- Run `qemu-system-i386 -cdrom cpos.iso` and you can use the system!

## Update

- Refactor the memory management.

## License

The project follows MIT license. Anyone can use it for free. See [LICENSE](LICENSE).

## Contributing

Welcome to create pull requests or issues to this project. I am really happy to see it!

### Contributors

* XIAOYI12 - xiaoyi12 OS Development
* QtLittleXu - XuYuxuan OS Document