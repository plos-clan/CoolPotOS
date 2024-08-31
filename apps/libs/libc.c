#include "../include/stdio.h"
#include "../include/syscall.h"

void put_char(char c){
    syscall_putchar(c);
}

void _start(){
    extern int main();
    int ret = main();
    syscall_exit(ret);
    while (1);
}