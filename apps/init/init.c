#include "../include/stdio.h"
#include "../include/cpos.h"

int main(int argc,char **argv){
    printf("Init service launched.\n");

    int pid = exec_elf("shell.bin","",false);
    if(pid != 0){
        printf("Shell process launched [PID: %d]\n",pid);
    } else printf("Cannot launch shell\n");
    return 0;
}
