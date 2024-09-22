OBJS_PACK = out/syscall.obj out/print.obj out/string.obj out/libc.obj out/math.obj out/timer.obj out/asmfunc.obj \
out/alloc/mman.obj out/alloc/pool.obj out/alloc.obj out/ttfprint.obj out/scanf.obj out/image.obj


default : $(OBJS_PACK)
	ar rv ../libo/libp.a $(OBJS_PACK)
out/%.obj : %.c Makefile
	gcc -m32 -I../include -nostdlib -fno-builtin -ffreestanding -fno-stack-protector -Qn -fno-pic -fno-pie -fno-asynchronous-unwind-tables -fomit-frame-pointer -march=pentium -O0 -w -c $*.c -o out/$*.obj
out/%.obj : %.cpp Makefile
	gcc -m32 -I../include -nostdinc -nostdlib -fno-builtin -ffreestanding -fno-stack-protector -Qn -fno-pic -fno-pie -fno-asynchronous-unwind-tables -fomit-frame-pointer -march=pentium -O0 -w -c $*.cpp -o out/$*.obj
out/%.obj : %.asm Makefile
	nasm -f elf $*.asm -o out/$*.obj

clean:
	rm out/*
	rm ../libo/libp.a