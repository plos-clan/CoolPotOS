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

## Build & Run

You need to install them on your computer:

- xmake
- xorriso
- QEMU
- git (for `GIT_VERSION` macro)
- clang
- lld (for linking LTO objects)
- openssl (kernel module key)
- python3 `cryptography` (sign kernel module)

### Options

You can use the command to set the target architecture and boot protocol (`x86_64` `limine` default):

```bash
xmake f --arch=<arch> --boot=<boot_protocol>
xmake build
xmake run os-pipeline
```

support target: `x86_64` `aarch64` `riscv64` `loongarch64` \
support boot protocol: `limine` `multiboot2` `other`

## License

This project is licensed under [MIT License](LICENSE).

## Contributing

Welcome to create pull requests or issues to this project. Then sit back and relax.

### Contributors

* Goto [CoolPotOS | Website](cpos.plos-clan.org) to see the contributors list.
