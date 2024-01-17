# CrashPowerDOS for x86

This operating system only runs on x86 platforms.
<br>
The new Python script build system replaces the old Makefile

> The source code has been rewritten because the memory management was completely broken in the previous version of the code.

<hr>

## How to build

* Download python and i686_elf_tools
* Enter `python build.py` in your shell
* Enter `grub-mkrescue -o cpos.iso isodir`

## How to run

* Enter `qemu-system-i386 -cdrom cpos.iso` in your shell to boot CrashPowerDOS

