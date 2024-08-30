#include "../include/syscall.h"

int gets(char *buf) {
    int index = 0;
    char c;
    while ((c = syscall_getc()) != '\n') {
        if (c == '\b') {
            if (index > 0) {
                index--;
                syscall_print("\b \b");
            }
        } else {
            buf[index++] = c;
            syscall_putchar(c);
        }
    }
    buf[index] = '\0';
    syscall_putchar(c);
    return index;
}

void hlt(){
    while (1);
}

int main(){
    syscall_print("Debug info\n");
    //char* info;
    //gets(info);
    //syscall_print(info);
    //put_char('A');
    hlt();
    return 0;
}