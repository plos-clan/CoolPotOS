<div align="center">
<img height="200px" src="https://github.com/user-attachments/assets/9542ad95-0f48-43ad-9617-a750db84e907" />

<h1 align="center">CoolPotOS</h1>
<h3>A simple toy operating system.</h3>

<img alt="GitHub License" src="https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub release (latest by date)" src="https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="GitHub issues" src="https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="Hardware" src="https://img.shields.io/badge/Hardware-i386_x64-blue?style=flat-square"/>
</div>

---

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

You need to install them on your computer:

- xmake
- xorriso
- QEMU
- NASM (i386 only)
- Zig (i386 only, auto installed by xmake)
- git (x86_64 only, for `GIT_VERSION` macro)
- clang (x86_64 only)
- lld (x86_64 only, for linking LTO objects)
- Rust nightly toolchain (x86_64 only)
- cbindgen (x86_64 only, use `cargo install cbindgen`)

### i386

Run `xmake run iso32` and it will build and run automatically.

### x86_64

Checkout submodules:

```bash
git submodule update --init --recursive
```

Then run `xmake run iso64` and it will build and run automatically.

## Development

To get syntax highlighting for your editor, you can generate a `compile_commands.json` file so that your editor knows how to find the source files:

```bash
xmake project -k compile_commands
```

## License

This project is licensed under [MIT License](LICENSE).

## Contributing

Welcome to create pull requests or issues to this project. Then sit back and relax.

### Contributors

- XIAOYI12 - OS Developer
- min0911Y - OS FileSystem Developer
- copi143 - UserHeap Developer
- QtLittleXu - XuYuxuan OS Document
- ViudiraTech - PCI Driver Optimization
- Vinbe Wan - IIC Developer
- A4-Tacks - Write Some Build Scripts
- wenxuanjun - OS Developer
- Minsecrus - Memory Usage Optimization
- CLimber-Rong - Software Developer
- shiyu - Debugger and Comments Writer
- 27Onion - Translated French README
- LY-Xiang - Optimized actions process
- suhuajun-github - Fix several bugs in the AHCI driver
- FengHeting - SMBIOS driver Developer
- blurryrect - OS Developer
