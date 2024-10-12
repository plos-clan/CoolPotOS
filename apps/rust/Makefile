
target := target/i686-unknown-none/release
apps := ../../isodir/apps

default:
ifneq (!$(wildcard libs),)
else
	mkdir libs
endif
	cp ../libo/libp.a libs/libp.a
	cargo build --release
	mv $(target)/cara $(apps)/cara.elf
	mv $(target)/os-terminal $(apps)/os-terminal.elf

clean:
	rm $(apps)/cara.elf
	rm $(apps)/os-terminal.elf
