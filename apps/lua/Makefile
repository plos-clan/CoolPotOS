include ../def.mk

BASIC_LIB_C = $(LIBS_PATH)/libp.a $(LIBS_PATH)/liblua.a

OBJS_PACK1 = out/lua.obj
OBJS_PACK2 = out/luac.obj
OBJS_PACK3 = out/lualib.obj

default : $(OBJS_PACK1) $(OBJS_PACK2) $(OBJS_PACK3)
	ar rv ../libo/liblua.a $(OBJS_PACK3)
	$(LINK) $(OBJS_PACK1) $(BASIC_LIB_C) -o ../../isodir/apps/lua.elf
	$(LINK) $(OBJS_PACK2) $(BASIC_LIB_C) -o ../../isodir/apps/luac.elf


out/%.obj : %.c Makefile
	$(C) -c $*.c -o out/$*.obj
out/%.obj : %.cpp Makefile
	$(CPP) -c $*.cpp -o out/$*.obj
out/%.obj : %.asm Makefile
	nasm -f elf_i386 $*.asm -o out/$*.obj


clean:
	rm out/*
	rm ../lipo/liblua.a
