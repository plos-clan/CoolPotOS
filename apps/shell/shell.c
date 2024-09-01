#include <stdio.h>
#include "../include/syscall.h"
#include "../include/string.h"

static int gets(char *buf, int buf_size) {
    int index = 0;
    char c;
    while ((c = syscall_getch()) != '\n') {
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

static int cmd_parse(char *cmd_str, char **argv, char token) {
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

int main(){
    printf("CoolPotOS UserShell v0.0.1\n");
    char com[100];
    char *argv[50];
    int argc = -1;
    char *buffer[255];
    while (1){
        syscall_get_cd(buffer);
        printf("%s$ ",buffer);
        if (gets(com, 100) <= 0) continue;

        argc = cmd_parse(com, argv, ' ');

        if (argc == -1) {
            printf("[Shell]: Error: out of arguments buffer\n");
            continue;
        }

        if (!strcmp("version", argv[0]))
            printf("CoolPotOS for x86 [v0.3.1]\n");
        else if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            printf("-=[CoolPotShell Helper]=-\n");
            printf("help ? h              Print shell help info.\n");
            printf("version               Print os version.\n");
            printf("type       <name>     Read a file.\n");
            printf("ls                    List all files.\n");
            printf("mkdir      <name>     Make a directory.\n");
            printf("del rm     <name>     Delete a file.\n");
            printf("sysinfo               Print system info.\n");
            printf("proc [kill<pid>|list] List all running processes.\n");
            printf("reset                 Reset OS.\n");
            printf("shutdown exit         Shutdown OS.\n");
            printf("debug                 Print os debug info.\n");
            printf("disk[list|<ID>|cg<ID>]List or view disks.\n");
            printf("cd  <path>            Change shell top directory.\n");
            printf("sb3       <name>      Player a wav sound file.\n");
            printf("exec <path>           Execute a application.\n");
        } else printf("\033ff3030;[Shell]: Unknown command '%s'.\033c6c6c6;\n", argv[0]);
    }
    return 0;
}