default: Makefile
	make -r -C libs
	make -r -C libutf
	make -r -C lib_pl_readline
	make -r -C init
	#make -r -C rust
	make -r -C shell

clean:
	rm ../isodir/apps/*.elf
	rm ../isodir/apps/*.bin
	make -r -C libs clean
	make -r -C libutf clean
	make -r -C lib_pl_readline clean
	#make -r -C rust clean
	make -r -C init clean
	make -r -C shell clean