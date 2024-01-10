BOOT := boot
KERNEL := kernel
INCLUDE := include/
OUT := target
SOURCE = $(wildcard kernel/*.c)

ASM := i686_elf_tools\bin\i686-elf-as
NASM := nasm
CC := i686_elf_tools\bin\i686-elf-gcc

OPTIONS := -I src/include/ -std=gnu99 -ffreestanding -O2 -c

.PHONY: ipl compile link install runBin runISO


ipl:
	$(ASM) $(BOOT)\boot.s -o $(OUT)\boot.o
	$(NASM) -f elf32 $(BOOT)\io.asm -o $(OUT)\io.o
	$(NASM) -f elf32 $(BOOT)\nasmfunc.asm -o $(OUT)\nasmfunc.o
	$(NASM) -f elf32 $(BOOT)\interrupt.asm -o $(OUT)\interrupt.o
	$(NASM) -f elf32 $(BOOT)\thread.asm -o $(OUT)\threadfunc.o

compile:
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\kernel.c -o target\kernel.o 
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\console.c -o target\console.o
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\gdt.c -o target\gdt.o 
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\idt.c -o target\idt.o
	$(CC) -I $(INCLUDE) $(OPTIONS) memory\memory.c -o target\memory.o
	$(CC) -I $(INCLUDE) $(OPTIONS) memory\page.c -o target\page.o
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\redpane.c -o target\redpane.o
	$(CC) -I $(INCLUDE) $(OPTIONS) drivers\keyboard.c -o target\keyboard.o
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\timer.c -o target\timer.o
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\kheap.c -o target\kheap.o
	$(CC) -I $(INCLUDE) $(OPTIONS) memory\queue.c -o target\queue.o
	$(CC) -I $(INCLUDE) $(OPTIONS) sysapp\shell.c -o target\shell.o
	$(CC) -I $(INCLUDE) $(OPTIONS) drivers\fatfs.c -o target\fatfs.o
	$(CC) -I $(INCLUDE) $(OPTIONS) memory\stack.c -o target\stack.o
	$(CC) -I $(INCLUDE) $(OPTIONS) drivers\acpi.c -o target\acpi.o
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\thread.c -o target\thread.o
	$(CC) -I $(INCLUDE) $(OPTIONS) drivers\disk.c -o target\disk.o
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\task.c -o target\task.o
	$(CC) -I $(INCLUDE) $(OPTIONS) kernel\common.c -o target\common.o

link:
	$(CC) -T linker.ld -o isodir\boot\cpos.bin -ffreestanding -O2 -nostdlib \
	 target\boot.o target\kernel.o target\console.o target\gdt.o target\idt.o \
	 target\redpane.o target\io.o target\memory.o target\page.o target\nasmfunc.o \
	 target\interrupt.o target\keyboard.o target\timer.o target\kheap.o target\queue.o \
	 target\shell.o target\fatfs.o target\stack.o target\acpi.o target\thread.o target\threadfunc.o \
	 target\disk.o target\task.o target\common.o

install:
	make ipl
	make compile
	make link

runBIN:
	qemu-system-i386 -kernel isodir/boot/cpos.bin -drive format=qcow2,file=cpos.qcow2

runISO:
	qemu-system-i386 -cdrom cpos.iso
