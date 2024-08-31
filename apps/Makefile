default: Makefile
	make -r -C libs
	make -r -C init
	make -r -C shell

clean:
	rm ../isodir/apps/*
	make -r -C libs clean
	make -r -C init clean
	make -r -C shell clean