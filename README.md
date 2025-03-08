
<br>
<br>

<h1 align="center">CoolPotOS</h1>

<div align="center"><strong>A simple toy operating system.</strong></div>

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

## Translations

- [English](README.md)
- [中文](README-zh-CN.md)

## Introduction

This is a simple operating system for x86_64 architecture.

## Module

* `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
* `liballoc` by wenxuanjun [plos-clan/liballoc](https://github.com/plos-clan/liballoc)

## Build & Run

### Environment

You need to install them on your computer:

- Xmake
- NASM (No x86_64)
- Clang (you can install manually if xmake cannot download it for you)
- Windows subsystem for Linux (Ubuntu 22.04)
    - xorriso
    - qemu-system-x86_64

### Steps

- Run `xmake run` on your terminal then it will build and run automatically

## License

The project follows MIT license. Anyone can use it for free. See [LICENSE](LICENSE).

## Contributing

Welcome to create pull requests or issues to this project. I am really happy to see it!

### Contributors

* XIAOYI12 - OS Development
* min0911Y - OS FileSystem Development
* copi143 - UserHeap Development
* QtLittleXu - XuYuxuan OS Document
* ViudiraTech - PCI driver optimization
* FengHeting - SMBIOS driver Development
* Vinbe Wan - IIC Development
* A4-Tacks - Write some build scripts
* wenxuanjun - OS Memory Manager Development
* Minsecrus - Memory usage optimization
* CLimber-Rong - Software Developer
