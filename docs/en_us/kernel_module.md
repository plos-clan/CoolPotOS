# Kernel Modules

## Introduction

Kernel modules are part of the operating system kernel, running in the same address space and privilege level as the kernel, with full kernel access rights. 
`CP_Kernel` kernel modules are `elf` format dynamically linked libraries with the extension `*.km`.

> Once a kernel module is loaded into the kernel, it cannot be unloaded and will only take effect at the next system restart.

Current kernel modules:

- `extfs` - ext file system implementation

## Source Code Directory

Located in the project's `module` directory, containing kernel module source code development header files and source code for each kernel module.

* all_include/cp_kernel.h: Common header files for kernel module development, including basic library functions and type encapsulation.
* all_include/fs_subsystem.h: Interface export for the file system subsystem.
* all_include/mem_subsystem.h: Interface export for the memory management subsystem.
* all_include/int_subsystem.h: Interface export for the interrupt and clock subsystem.
* all_include/driver_subsystem.h: Interface export for the device management subsystem.
* all_include/errno.h: Error code definitions.
* all_include/lock.h: Lock types and interface definitions.

## Examples

The source code for the `extfs` kernel module is located in the `module/extfs` directory. Other kernel modules can refer to this project as a template to create new kernel modules in the `module` directory.