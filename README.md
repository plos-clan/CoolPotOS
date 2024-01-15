# CrashPowerDOS for x86

This operating system only runs on x86 platforms.

> The source code has been rewritten because the memory management was completely broken in the previous version of the code.

<hr>

## How to build

* Download python
* Start your shell application
* Type `python build.py`
* Type `grub-mkrescue -o cpos.iso isodir`

## How to run

* Type `qemu-system-i386 -cdrom cpos.iso`

