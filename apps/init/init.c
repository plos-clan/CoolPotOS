#include "../include/stdio.h"

void hlt(){
    while (1);
}

int main(){
    put_char('A');
    hlt();
    return 0;
}