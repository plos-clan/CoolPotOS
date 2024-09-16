default: Makefile
	make -r -C libs
	make -r -C libutf
	make -r -C lib_pl_readline
	make -r -C init
	make -r -C shell
	make -r -C lua
	make -r -C phigrOS

clean:
	rm ../isodir/apps/*.elf
	rm ../isodir/apps/*.bin
	make -r -C libs clean
	make -r -C libutf clean
	make -r -C lib_pl_readline clean
	make -r -C init clean
	make -r -C shell clean
	make -r -C lua clean
	make -r -C phigrOS clean