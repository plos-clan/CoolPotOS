# CP_Kernel Subsystems

## Introduction

Currently, the CPOS kernel has the following subsystems:

- Multi-tasking management subsystem
- Memory management subsystem
- Interrupt and clock subsystem
- Device management subsystem
- File system subsystem
- Services and modules (not subsystems, but functional components)

## Process Subsystem (Multi-tasking Management Subsystem)

Its source code is located in the `task` directory, responsible for process/thread creation, suspension, destruction, process signals and communication, and also includes the scheduler implementation.

- eevdf.c: Core implementation of the Earliest Eligible Virtual Deadline First scheduling algorithm.
- futex.c: Implementation of thread suspension/wakeup mechanism.
- ipc.c: Implementation of inter-process communication mechanism.
- prsys.c: Implementation of system calls related to multi-tasking, such as fork, exec, clone, etc.
- signal.c: Implementation of signal mechanism. (TODO: Not yet completed)
- scheduler.c: General encapsulation part of the scheduler, decoupled from specific algorithm implementations.
- poll.c: Implementation of I/O multiplexing mechanism.

## Memory Subsystem (Memory Management Subsystem)

Its source code is located in the `mem` directory, including physical page frame allocation management, memory statistics, kernel heap allocator implementation, and lazy allocator implementation.

- heap/: Kernel heap allocator implementation, the `plalloc` allocator written by `copi143`.
- lazyalloc.c: Lazy allocator implementation.
- frame.c: Physical page frame allocator implementation.
- memstats.c: Memory statistics implementation.
- page.c: Page table management implementation.
- bitmap.c: Bitmap implementation, page frame allocator uses bitmap to manage physical memory.
- hhdm.c: Related to high half direct mapping memory offset acquisition and some virtual-physical address conversion.

## Interrupt and Clock Subsystem

Its source code exists in both `cpu` and `driver` directories and is not an independent subsystem.

- cpu/error_handle.c: Kernel exception handling implementation.
- cpu/idt.c: Interrupt descriptor table initialization and interrupt handler registration.
- cpu/empty_interrupt.c: Debug check for interrupt handlers that do not exist.
- cpu/cpustats.c: High-precision real-time counter implementation (provides the scheduler with a real-time clock source).
- driver/acpi/hpet.c: High Precision Event Timer (HPET) driver, calibrates high-precision real-time counter and `local apic` timer.
- driver/acpi/apic.c: Initialization of the Advanced Programmable Interrupt Controller (including initialization of `local apic` and `I/O apic`).

## Device Subsystem (Device Management Subsystem)

Its source code is located in the `driver` directory, responsible for device registration and management, including common device driver implementations.

- /fs/devfs.c: Device file system implementation, which is a mapping of the device list and also provides some device operation interfaces to user mode and the kernel itself.
- pci/: PCI bus and its associated device driver implementations.
- acpi/: Advanced Configuration and Power Interface (ACPI) related driver implementations, including power management.
- iic/: I2C bus and its associated device driver implementations (TODO: Not yet tested).
- input/: Input device driver implementations, including keyboard and mouse drivers.
- sound/: Built-in sound card driver implementations, currently only includes `PC Speaker` and `sb16` drivers.
- dma.c: DMA controller driver implementation.
- rtc.c: CMOS chip driver implementation, providing real-time clock functionality.
- gop.c: Basic Graphics Output Protocol driver implementation, providing simple graphics output functionality.
- serial.c: Serial port driver implementation, providing serial communication functionality.
- tty.c: Terminal device driver implementation, providing character terminal functionality.
- urandom.c: Random number device driver implementation, providing pseudo-random number generation functionality.
- zero.c: Zero device and null device driver implementations, providing read-zero and write-discard functionality.
- vdisk.c: Device manager implementation, whose operation interfaces and device information are represented by `devfs`.

## File System Subsystem

Its source code is located in the `fs` directory, responsible for file system registration and management, and has built-in implementations of `fatfs` and `iso9660` file systems.

- vfs.c: Virtual file system management, providing file system registration and mounting, and encapsulating file operation interfaces.
- fatfs/: `FAT` file system implementation.
- devfs.c: Device file system implementation, part of the device management subsystem.
- iso9660.c: `ISO9660` file system implementation.
- tmpfs.c: Temporary file system implementation, providing a memory-based file system.
- pipe.c: Pipe implementation, providing a pipe mechanism for inter-process communication.
- modfs.c: Module file system implementation, providing kernel module loading and management functionality.
- partition.c: Partition management implementation, providing functionality for identifying and operating disk partitions, and mounting the identified partitions as virtual block devices to `devfs`.

## Services and Modules

Its source code is located in the `service` and `kmod` directories and is not an independent subsystem but a functional component of the kernel.

- service/killer.c: Kernel service responsible for recycling zombie processes.
- service/syscall.c: Kernel service providing system call interfaces.
- service/sysuser.c: Kernel service providing session and permission management.
- kmod/dlinker.c: Dynamic linker implementation, providing dynamic loading and symbol resolution functionality for kernel modules.
- kmod/module.c: Encapsulates kernel modules loaded by the bootloader and mounts files to `modfs`.
- kmod/interface.c: Provides interfaces for kernel modules to interact with the kernel itself.