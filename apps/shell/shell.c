#include <stdio.h>
#include "../include/syscall.h"
#include "../include/string.h"
#include "../include/cpos.h"

extern void print_info();

static int gets(char *buf) {

    int index = 0;
    char c;

    while ((c = getc()) != '\n') {
        if (c == '\b') {
            if (index > 0) {
                index--;
                printf("\b \b");
            }
        } else {
            buf[index++] = c;
            printf("%c",c);
        }
    }
    buf[index] = '\0';
    printf("%c",c);
    return index;
}

static inline int cmd_parse(char *cmd_str, char **argv, char token) {
    int arg_idx = 0;
    while (arg_idx < 50) {
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char *next = cmd_str;
    int argc = 0;

    while (*next) {
        while (*next == token) *next++;
        if (*next == 0) break;
        argv[argc] = next;
        while (*next && *next != token) *next++;
        if (*next) *next++ = 0;
        if (argc > 50) return -1;
        argc++;
    }

    return argc;
}

int main(int argc_v,char **argv_v){

    printf("CoolPotOS UserShell v0.0.1\n");
    char com[100];
    char *argv[50];
    int argc = -1;
    char *buffer[255];

    while (1){
        syscall_get_cd(buffer);
        printf("\033[40m;default@localhost: \03330m;%s\\\03321m;$ ",buffer);

        if (gets(com) <= 0) continue;

        char* com_copy[100];
        strcpy(com_copy,com);

        argc = cmd_parse(com, argv, ' ');

        if (argc == -1) {
            printf("[Shell]: Error: out of arguments buffer\n");
            continue;
        }

        if (!strcmp("version", argv[0])){
            print_info();
        }else if (!strcmp("system", argv[0])){
            exit(0);
        }else if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            printf("-=[CoolPotShell Helper]=-\n");
            printf("help ? h              Print shell help info.\n");
            printf("version               Print os version.\n");
            printf("system                Launch system shell.\n");
        } else {
            int pid = exec_elf(argv[0],com_copy,false);
            if(pid == NULL){
                printf("\033ff3030;[Shell]: Unknown command '%s'.\033c6c6c6;\n", argv[0]);
            }
        }
    }
}