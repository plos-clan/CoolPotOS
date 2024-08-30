#include "../include/syscall.h"

void hlt(){
    while (1);
}

int main(){
    syscall_print("Hello! User Application!\n");
    //put_char('A');
    hlt();
    return 0;
}