# CrashPowerDOS

该操作系统仅支持运行在x86平台

---

## 准备环境

* make 构建工具
* Windows Subsystem for Linux
    * `sudo apt install xorriso`
    * `sudo apt install grub-pc`
* 您需要下载`i686_elf_tools`编译工具链到项目目录

## 如何构建

1. cd CrashPowerOS
2. make install
3. 在WSL中输入`grub-mkrescue -o cpos.iso isodir`

## 如何运行

* 项目根目录会生成cpos.iso文件
* qemu: `qemu-system-i386 -cdrom cpos.iso`
* VMware可直接装载引导
