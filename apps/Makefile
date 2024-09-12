default: Makefile
	make -r -C libs
	make -r -C libutf
	make -r -C init
	make -r -C shell
	make -r -C lua

clean:
	rm ../isodir/apps/*.elf
	rm ../isodir/apps/*.bin
	make -r -C libs clean
	make -r -C libutf clean
	make -r -C init clean
	make -r -C shell clean
	make -r -C lua clean