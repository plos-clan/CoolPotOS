#include "../include/stdio.h"
#include "../include/cpos.h"

int main(){
    printf("Init service launched.\n");
    exec_elf("shell.bin");
    return 0;
}
