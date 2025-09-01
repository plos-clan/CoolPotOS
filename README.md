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
- oib (x86_64 & build img only, use `cargo install oib`)

### Options

You can use the command to set the target architecture (`x86_64` default):

```bash
xmake f -y --arch=i686
```

### Commands

- `xmake run` - Build and run the **ISO** image.
- `xmake build iso` - Build a bootable ISO image.
- `xmake build img` - Build a bootable disk image (x86_64 only).

## Development

You can generate a `compile_commands.json` file by:

```bash
xmake project -k compile_commands
```

Then your editor knows how to find the source files and derives syntax highlighting.

## License

This project is licensed under [MIT License](LICENSE).

## Feature

### AMD64

Based on UEFI BIOS boot. \
Use Limine bootloader.

- [x] 4 Level Page Table Memory Management
- [x] xAPIC and x2APIC
- [x] Kernel Module
- [x] AHCI Disk Driver
- [x] Multi-Task (Process and Thread)
- [x] PS/2 keyboard and Mouse
- [x] PCIE Device Enum
- [x] ACPI Power Management
- [x] VFS VDisk interface
- [x] IPC Message Queue
- [ ] Process Signal
- [x] Multiprocessor Scheduler
- [x] User program
- [x] Device File System
- [x] Floating Point Unit
- [ ] IIC Driver
- [ ] Nvme and USB Driver
- [ ] PCNET and Rtl8169 Driver
- [x] SB16 and PCSpeaker Driver
- [x] TTY Driver
- [x] SATA/SATAPI Driver

## Contributing

Welcome to create pull requests or issues to this project. Then sit back and relax.

### Contributors

* Goto [CoolPotOS | Website](cpos.plos-clan.org) to see the contributors list.
