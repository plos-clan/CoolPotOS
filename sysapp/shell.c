#include "../include/shell.h"
#include "../include/io.h"
#include "../include/queue.h"
#include "../include/console.h"
#include "../include/common.h"
#include "../include/redpane.h"
#include "../include/acpi.h"
#include "../include/kheap.h"
#include "../include/fatfs.h"

#include <stddef.h>

extern Queue *key_char_queue;

char getc() {
    while (key_char_queue->size == 0x00) {
        io_hlt();
    }
    return queue_pop(key_char_queue);
}

int gets(char *buf, int buf_size) {
    int index = 0;
    char c;
    while ((c = getc()) != '\n') {
        if (c == '\b') {
            if (index > 0) {
                index--;
                terminal_writestring("\b \b");
            }
        } else {
            buf[index++] = c;
            terminal_putchar(c);
        }
    }
    buf[index] = '\0';
    terminal_putchar('\n');

    return index;
}

int cmd_parse(char *cmd_str, char **argv, char token) {
    int arg_idx = 0;
    while (arg_idx < MAX_ARG_NR) {
        argv[arg_idx] = NULL;
        arg_idx++;
    } // end of while
    char *next = cmd_str;
    int argc = 0;
    while (*next) {
        while (*next == token) *next++;
        if (*next == 0) break;
        argv[argc] = next;
        while (*next && *next != token) *next++;
        if (*next) {
            *next++ = 0;
        }
        if (argc > MAX_ARG_NR) return -1;
        argc++;
    }
    return argc;
}

void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i == 1) terminal_writestring("");
        else terminal_writestring(" ");
        terminal_writestring(argv[i]);
    }
    terminal_writestring("\n");
}

void cmd_ls(int argc,char **argv){
    struct File *file_infos = (struct File*)kmalloc(sizeof(struct File) * 16);
    int files = read_root_dir(file_infos);
    int f = 0,d = 0;

    println(" ");
    for (int i = 0; i < files; ++i) {
        struct File info = file_infos[i];
        if(!strcmp(info.name,"\0")) continue;
        if(info.type == 0x20){
            printf("%s   %d   <FILE>\n",info.name,info.size);
            f++;
        } else if(info.type == 0x10) {
            printf("%s         <DIR>\n",info.name,info.size);
            d++;
        }
    }
    printf("\tAll Files: %d  | All Directory: %d ",f,d);
    println(" ");

    kfree(file_infos);
}

void cmd_mkdir(int argc,char **argv){
    if(argc == 1) {
        println("[Shell-MKDIR]: If there are too few parameters, please specify the folder name.");
        return;
    }
    struct File *dir = create_dir(argv[1]);
    if(dir == NULL){
        println("[Shell-MKDIR]: Cannot create directory.");
    }
    kfree(dir);
}

void cmd_del(int argc,char **argv){
    if(argc == 1) {
        println("[Shell-DEL]: If there are too few parameters, please specify the folder name.");
        return;
    }
    struct File *info = open_file(argv[1]);
    if(info == NULL){
        printf("[Shell-DEL]: Not found [%s] \n",argv[1]);
        return;
    }
    delete_file(info);
}

void cmd_exit(){
    acpi_shutdown();
}

void shell_setup() {
    terminal_writestring("CrashPowerDOS for x86 [Version ");
    terminal_writestring(CPOS_VERSION);
    terminal_writestring("]\n");
    println("Copyright 2023-2024 by XIAOYI12.");

    char com[MAX_COMMAND_LEN];
    char *argv[MAX_ARG_NR];
    int argc = -1;

    while (1) {
        terminal_writestring("CPOS~[system-KERNEL]:/$ ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;

        argc = cmd_parse(com, argv, ' ');
        if (argc == -1) {
            println("[shell]: error: number of arguments exceed MAX_ARG_NR(30)");
            continue;
        }

        if (!strcmp("version", argv[0])) {
            terminal_writestring("CrashPowerDOS x86-no-desktop ");
            println(CPOS_VERSION);
        } else if (!strcmp("echo", argv[0])) {
            cmd_echo(argc, argv);
        } else if ((!strcmp("?", argv[0])) || (!strcmp("help", argv[0])) || (!strcmp("h", argv[0]))) {
            println("[[CrashPowerDOS Shell Helper]]");
            println("version      Print the shell version.");
            println("clear        Clear the screen.");
            println("exit         Exit CrashPowerDOS.");
            println("echo <msg>   Print a message to shell console.");
            println("ls           List this dir all files.");
            println("mkdir <name> Create a directory.");
            println("del <name>   Delete a file or directory.");
            println("\n");
        } else if(!strcmp("clear",argv[0])) {
            terminal_clear();
        } else if(!strcmp("exit",argv[0])) {
            cmd_exit();
        } else if(!strcmp("ls",argv[0])) {
            cmd_ls(argc, argv);
        }else if(!strcmp("mkdir",argv[0])) {
            cmd_mkdir(argc, argv);
        } else if((!strcmp("del",argv[0]))||(!strcmp("delete",argv[0]))){
            cmd_del(argc,argv);
        } else {
            terminal_writestring("[shell]: Unknown command '");
            terminal_writestring(com);
            println("'.");
        }
    }
}

