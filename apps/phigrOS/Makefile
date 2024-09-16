OBJS_PACK = out/csp_main.obj
include ../def.mk

default: $(OBJS_PACK)
	$(LINK) $(OBJS_PACK) $(BASIC_LIB_C) -o ../../isodir/sys/csp.bin
out/%.obj : %.c Makefile
	$(C) -c $*.c -o out/$*.obj
out/%.obj : %.cpp Makefile
	$(CPP) -c $*.cpp -o out/$*.obj
out/%.obj : %.asm Makefile
	nasm -f elf_i386 $*.asm -o out/$*.obj

clean:
	rm out/*