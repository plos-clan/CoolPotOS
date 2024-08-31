CFLAGS = -m32 -I$(INCLUDE_PATH) -nolibc -nostdlib -ffreestanding -fno-stack-protector -Qn -fno-pic -fno-pie -fno-asynchronous-unwind-tables -fomit-frame-pointer -finput-charset=UTF-8 -fexec-charset=GB2312 -march=pentium -Qn -O0 -w
CPPFLAGS = -m32 -I$(INCLUDE_PATH) -nostdinc -nolibc -nostdlib -ffreestanding -fno-exceptions -fno-stack-protector -Qn -fno-pic -fno-pie -fno-asynchronous-unwind-tables -fomit-frame-pointer -finput-charset=UTF-8 -fexec-charset=GB2312 -Qn -O3 -march=pentium -fno-rtti -w

CC = gcc

C = $(CC) $(CFLAGS)
CPP = $(CC) $(CPPFLAGS)
INCLUDE_PATH := ../include
NASM = nasm
LIBS_PATH := ../libo

LD = ld
LD_FLAGS = -Ttext 0xb0000010 -m elf_i386 -static -e _start
LINK = $(LD) $(LD_FLAGS)

BASIC_LIB_C = $(LIBS_PATH)/libp.a