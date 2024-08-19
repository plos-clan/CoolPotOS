#include "../include/stdio.h"

void hlt(){
    while (1);
}

void putc(char c){
    asm ("push %%eax\n"
         "push %%edx\n"
         "mov %0,%%edx\n"
         "mov $0x1,%%eax\n"
         "int $31\n"
         "pop %%edx\n"
         "pop %%eax\n"::"m"(c));
}

int main(){
    //putc('A');
    put_char();
    hlt();
    return 0;
}