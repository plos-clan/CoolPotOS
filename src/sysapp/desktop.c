#include "../include/desktop.h"
#include "../include/task.h"
#include "../include/printf.h"

int send_ok_box(char *title,char* message){

}

static int desktop_setup(){



    for (;;) {

    }
    return 0;
}

void menu_sel(){
    klogf(true,"Launching kernel desktop service...\n");
    kernel_thread(desktop_setup,NULL,"CPOS-Desktop");
}
