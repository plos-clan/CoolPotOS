REM install "cygwin" to use command 'dd'

make install


REM grub-2.02-for-windows\grub-mkrescue -o cpos.iso isodir

qemu-system-i386 -kernel isodir/boot/grub/cpos.bin