#include "../include/shell.h"
#include "../include/queue.h"
#include "../include/graphics.h"
#include "../include/common.h"
#include "../include/task.h"
#include "../include/cmos.h"
#include "../include/fat16.h"
#include "../include/timer.h"
#include "../include/io.h"

extern Queue *key_char_queue;

char getc() {
    while (key_char_queue->size == 0x00) {
        printf("");
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
                vga_writestring("\b \b");
            }
        } else {
            buf[index++] = c;
            vga_putchar(c);
        }
    }
    buf[index] = '\0';
    vga_putchar(c);
    return index;
}

int cmd_parse(char *cmd_str, char **argv, char token) {
    int arg_idx = 0;
    while (arg_idx < MAX_ARG_NR) {
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
        if (argc > MAX_ARG_NR) return -1;
        argc++;
    }

    return argc;
}

void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i == 1) vga_writestring("");
        else vga_writestring(" ");
        vga_writestring(argv[i]);
    }
    vga_putchar('\n');
}

void cmd_proc(int argc, char **argv){
    if (argc <= 1) {
        printf("\033[Shell-PROC]: If there are too few parameters.\036\n");
        return;
    }

    if(!strcmp("list",argv[1])){
        print_proc();
    } else if(!strcmp("kill",argv[1])){
        if (argc <= 2) {
            printf("\033[Shell-PROC-kill]: If there are too few parameters.\036\n");
            return;
        }
        int pid = strtol(argv[2],NULL,10);
        task_kill(pid);
    } else{
        printf("\033[Shell-[PROC]]: Unknown parameter\036\n");
        return;
    }
}

void cmd_date(){
    printf("System Time:           %s\n",get_date_time());
    printf("Memory Usage: %dKB | All Size: %dMB\n",memory_usage()/1024,(KHEAP_START+KHEAP_INITIAL_SIZE)/1024/1024);
    print_cpu_id();
    vga_writestring("\n");
}

void cmd_ls() {
    struct File *root = (struct File *) kmalloc(sizeof(struct File) * MAX_FILE_NUM);
    int files = read_root_dir(root);
    int index = 0, size = 0;

    for (int i = 0; i < files; ++i) {
        struct File file = root[i];
        if (!strcmp(file.name, "\0")) continue;
        printf("%s   %s  %d\n", file.name, file.type == 0x20 ? "<FILE>" : " <DIR> ", file.size);
        index++;
        size += file.size;
    }
    printf("    All File: %d  | All Size: %dByte\n", index, size);
    kfree(root);
}

void cmd_cat(int argc, char **argv) {
    if (argc <= 2) {
        printf("\033[Shell-CAT]: If there are too few parameters, please specify the filename and util.\036\n");
        return;
    }
    struct File *file = open_file(argv[1]);
    if (file == NULL) {
        printf("\033[Shell-CAT]: Not found [%s]\036 \n", argv[1]);
        return;
    }

    char *buffer[1024] = {0};

    for (int i = 2; i < argc; i++) {
        if (i == 2) strcat(buffer, "");
        else strcat(buffer, " ");
        strcat(buffer, argv[i]);
    }

    save_file(file, buffer);
    kfree(file);
}

void cmd_read(int argc, char **argv) {
    if (argc == 1) {
        printf("\033[Shell-READ]: If there are too few parameters, please specify the filename.\036\n");
        return;
    }
    struct File *file = open_file(argv[1]);
    char *buffer = (char *) kmalloc(sizeof(char) * 4096);
    if (file == NULL) {
        printf("\033[Shell-READ]: Not found [%s]\036 \n", argv[1]);
        return;
    }

    read_file(file, buffer);
    printf("%s\n", buffer);
    kfree(buffer);
    kfree(file);
}

void cmd_mkdir(int argc, char **argv) {
    if (argc == 1) {
        printf("\033[Shell-MKDIR]: If there are too few parameters, please specify the directory name.\036\n");
        return;
    }
    printf("Create directory: %s\n",argv[1]);
    struct File *dir = create_dir(argv[1]);
    if (dir == NULL) {
        printf("\033[Shell-MKDIR]: Cannot create directory '%s'.\036\n", argv[1]);
        return;
    }
    kfree(dir);
}

void cmd_del(int argc, char **argv) {
    if (argc == 1) {
        vga_writestring("\033[Shell-DEL]: If there are too few parameters, please specify the folder name.\036\n");
        return;
    }
    struct File *info = open_file(argv[1]);
    if (info == NULL) {
        printf("\033[Shell-DEL]: Not found [%s]\036 \n", argv[1]);
        return;
    }
    delete_file(info);
    kfree(info);
}

void cmd_reset(){
    reset_kernel();
}

void setup_shell(){
    vga_clear();
    printf("%s for x86 [Version %s] \n",OS_NAME, OS_VERSION);
    printf("\032Copyright 2024 XIAOYI12 (Build by GCC i686-elf-tools)\036\n");

    char com[MAX_COMMAND_LEN];
    char *argv[MAX_ARG_NR];
    int argc = -1;

    while (1) {
        printf("\035CPOS/>\036 ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        argc = cmd_parse(com, argv, ' ');

        if (argc == -1) {
            vga_writestring("[Shell]: Error: out of arguments buffer\n");
            continue;
        }

        if (!strcmp("version", argv[0]))
            printf("%s for x86 [%s]\n",OS_NAME, OS_VERSION);
        else if (!strcmp("echo", argv[0]))
            cmd_echo(argc, argv);
        else if (!strcmp("clear", argv[0]))
            vga_clear();
        else if (!strcmp("proc", argv[0]))
            cmd_proc(argc, argv);
        else if (!strcmp("sysinfo", argv[0]))
            cmd_date();
        else if (!strcmp("ls", argv[0]))
            cmd_ls();
        else if (!strcmp("cat", argv[0]))
            cmd_cat(argc, argv);
        else if (!strcmp("read", argv[0]))
            cmd_read(argc, argv);
        else if (!strcmp("mkdir", argv[0]))
            cmd_mkdir(argc, argv);
        else if (!strcmp("del", argv[0]) || !strcmp("rm", argv[0]))
            cmd_del(argc, argv);
        else if (!strcmp("reset", argv[0]))
            cmd_reset();
        else if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            vga_writestring("-=[\037CrashPowerShell Helper\036]=-\n");
            vga_writestring("help ? h              \032Print shell help info.\036\n");
            vga_writestring("version               \032Print os version.\036\n");
            vga_writestring("echo       <msg>      \032Print message.\036\n");
            vga_writestring("ls                    \032List all files.\036\n");
            vga_writestring("cat <name> <util>     \032Edit a file.\036\n");
            vga_writestring("read       <name>     \032Read a file.\036\n");
            vga_writestring("mkdir      <name>     \032Make a directory.\036\n");
            vga_writestring("del rm     <name>     \032Delete a file.\036\n");
            vga_writestring("sysinfo               \032Print system info.\036\n");
            vga_writestring("proc [kill<pid>|list] \032Lists all running processes.\036\n");
            vga_writestring("reset                 \032Reset OS.\036\n");
        } else printf("\033[Shell]: Unknown command '%s'.\036\n", argv[0]);
    }
}
