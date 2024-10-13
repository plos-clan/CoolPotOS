#include "../include/bootarg.h"
#include "../include/common.h"
#include "../include/printf.h"

uint32_t boot_arg_device_d = 0;

static device_parser(char* arg){
    char* tokens[2];
    tokens[0] = strtok(arg,"=");
    tokens[1] = strtok(NULL,"=");

    if(!strcmp(tokens[0],"tty")){
        if(!strcmp(tokens[1],"default")){
            boot_arg_device_d |= TTY_DEFAULT;
        } else if(!strcmp(tokens[1],"os_terminal")){
            boot_arg_device_d |= TTY_OS_TERMINAL;
        } else{
            boot_arg_device_d |= TTY_UNKNOWN;
        }
    }
}

static void splitString1(char *str) {
    const char *delimiter = " ";
    char *token = strtok(str, delimiter);
    while (token != NULL) {
        device_parser(token);
        token = strtok(NULL, delimiter);
    }
}

void boot_arg(char *args) {
    splitString1(args);
}