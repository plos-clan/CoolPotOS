default: Makefile
	make -r -C libs
	make -r -C init
	make -r -C base_service

clean:
	rm ../isodir/apps/*
	make -r -C libs clean
	make -r -C init clean
	make -r -C base_service clean