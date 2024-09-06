#include "../include/stdio.h"
#include "../include/cpos.h"

int main(){
    printf("Init service launched.\n");
    syscall_vfs_change_path("apps");
    int pid = exec_elf("shell.bin");
    if(pid != -1){
        printf("Shell launch win! PID:[%d]\n",pid);
    } else printf("Error: Cannot launch shell\n");
    return 0;
}
