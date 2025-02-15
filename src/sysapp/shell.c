#include "shell.h"
#include "limine.h"
#include "smp.h"
#include "kprint.h"
#include "krlibc.h"
#include "timer.h"
#include "scheduler.h"
#include "keyboard.h"
#include "pcie.h"
#include "cpuid.h"
#include "gop.h"
#include "heap.h"
#include "pcb.h"
#include "vfs.h"
#include "sprintf.h"

extern void cp_shutdown();
extern void cp_reset();

char *shell_work_path;
extern uint64_t memory_size; //hhdm.c
char com_copy[100];

static inline int isprint_syshell(int c) {
    return (c > 0x1F && c < 0x7F);
}

static char getc() {
    char c;
    do {
        c = kernel_getch();
        if (c == '\b' || c == '\n') break;
    } while (!isprint_syshell(c));
    return c;
}

static int gets(char *buf, int buf_size) {
    int index = 0;
    char c;
    while ((c = getc()) != '\n') {
        if (c == '\b') {
            if (index > 0) {
                index--;
                printk("\b \b");
            }
        } else {
            buf[index++] = c;
            printk("%c", c);
        }
    }
    buf[index] = '\0';
    printk("%c", c);
    return index;
}

int ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) {
        return 0;
    }

    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

static int cmd_parse(char *cmd_str, char **argv, char token) {
    int arg_idx = 0;
    while (arg_idx < MAX_ARG_NR) {
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char *next = cmd_str;
    int argc = 0;

    while (*next) {
        while (*next == token) UNUSED(*next++);
        if (*next == 0) break;
        argv[argc] = next;
        while (*next && *next != token) UNUSED(*next++);
        if (*next) *next++ = 0;
        if (argc > MAX_ARG_NR) return -1;
        argc++;
    }

    return argc;
}

static void shutdown_os() {
    cp_shutdown();
}

static void reboot_os() {
    cp_reset();
}

static void cd(int argc, char **argv) {
    if (argc == 1) {
        printk("If there are too few parameters.\n");
        return;
    }
    char *s = com_copy + 3;
    if (s[strlen(s) - 1] == '/' && strlen(s) > 1) { s[strlen(s) - 1] = '\0'; }
    if (streq(s, ".")) return;
    if (streq(s, "..")) {
        if (streq(s, "/")) return;
        char *n = shell_work_path + strlen(shell_work_path);
        while (*--n != '/' && n != shell_work_path) {}
        *n = '\0';
        if (strlen(shell_work_path) == 0) strcpy(shell_work_path, "/");
        return;
    }
    char *old = strdup(shell_work_path);
    if (s[0] == '/') {
        strcpy(shell_work_path, s);
    } else {
        if (streq(shell_work_path, "/"))
            sprintf(shell_work_path, "%s%s", shell_work_path, s);
        else
            sprintf(shell_work_path, "%s/%s", shell_work_path, s);
    }
    if (vfs_open(shell_work_path) == NULL) {
        printk("cd: %s: No such directory\n", s);
        sprintf(shell_work_path, "%s", old);
        free(old);
    }
}

static void mkdir(int argc, char **argv) {
    if (argc == 1) {
        printk("[Shell-MKDIR]: If there are too few parameters.\n");
        return;
    }
    char *buf_h = com_copy + 6;
    char bufx[100];
    if (buf_h[0] != '/') {
        if(!strcmp(shell_work_path,"/"))
            sprintf(bufx, "/%s", buf_h);
        else sprintf(bufx, "%s/%s", shell_work_path, buf_h);
    } else sprintf(bufx, "%s", buf_h);
    if (vfs_mkdir(bufx) == -1) {
        printk("Failed create directory [%s].\n", argv[1]);
    }
}

void ps() {
    extern pcb_t kernel_head_task;
    pcb_t longest_name = kernel_head_task;
    size_t longest_name_len = 0;
    while (longest_name != NULL) {
        if (strlen(longest_name->name) > longest_name_len)
            longest_name_len = strlen(longest_name->name);
        longest_name = longest_name->next;
        if(longest_name->pid == kernel_head_task->pid) break;
    }
    pcb_t pcb = kernel_head_task;

    uint64_t all_time = 0;
    uint64_t idle_time = kernel_head_task->cpu_timer;
    uint64_t mem_use = 0;
    uint32_t bytes = get_all_memusage();
    int memory = (bytes > 10485760) ? bytes / 1048576 : bytes / 1024;
    printk("PID  %-*s  RAM(byte)  Priority  Timer     ProcessorID\n", longest_name_len, "NAME");
    while (pcb != NULL) {
        all_time += pcb->cpu_timer;
        if(pcb->task_level != TASK_KERNEL_LEVEL) mem_use += pcb->mem_usage;
        printk("%-5d%-*s  %-10d %-10d%-10dCPU%-d\n", pcb->pid, longest_name_len, pcb->name,
               pcb->mem_usage,
               pcb->task_level, pcb->cpu_clock,pcb->cpu_id);
        pcb = pcb->next;
        if(pcb->pid == kernel_head_task->pid) break;
    }
    printk("   --- CPU Usage: %d%% | Memory Usage: %d%s/%dMB ---\n",idle_time * 100 / all_time,mem_use + memory,bytes > 10485760 ? "MB" : "KB",memory_size / 1024 / 1024);
}

static void ls(int argc, char **argv) {
    vfs_node_t p;
    if (argc == 1) {
        p = vfs_open(shell_work_path);
    } else {
        char *buf_h = com_copy + 3;
        char bufx[100];
        if (buf_h[0] != '/') {
            if(!strcmp(shell_work_path,"/"))
                sprintf(bufx, "/%s", buf_h);
            else sprintf(bufx, "%s/%s", shell_work_path, buf_h);
        } else sprintf(bufx, "%s", buf_h);
        p = vfs_open(bufx);
    }
    if (p == NULL) {
        printk("Cannot fount directory.\n");
        return;
    }
    list_foreach(p->child, i) {
        vfs_node_t c = (vfs_node_t) i->data;
        printk("%s ", c->name);
    }
    printk("\n");
}

static void pkill(int argc, char **argv) {
    if (argc == 1) {
        printk("[Shell-PKILL]: If there are too few parameters.\n");
        return;
    }
    int pid = strtol(argv[1], NULL, 10);
    pcb_t pcb = found_pcb(pid);
    if (pcb == NULL) {
        printk("Cannot find procces [%d]\n", pid);
        return;
    }
    kill_proc(pcb);
}

static void echo(int argc,char** argv){
    if (argc == 1) {
        printk("[Shell-ECHO]: If there are too few parameters.\n");
        return;
    }
    vfs_node_t stdout = vfs_open("/dev/stdout");
    if(stdout == NULL)
        printk("ERROR: stream device is null.\n");
    else{
        char* buf = com_copy + 5;
        if(vfs_write(stdout,buf,0, strlen(buf)) == VFS_STATUS_FAILED)
            printk("stdout stream has error.\n");
        vfs_close(stdout);
        printk("\n");
    }
}

static void sys_info() {
    cpu_t cpu = get_cpu_info();

    uint32_t bytes = get_all_memusage();
    int memory = (bytes > 10485760) ? bytes / 1048576 : bytes / 1024;
    extern bool is_pcie;

    printk("        -*&@@@&*-        \n");
    printk("      =&@@@@@@@@@:\033[36m-----\033[39m          -----------------\n");
    printk("    .&@@@@@@@@@@:\033[36m+@@@@@:\033[39m         OSName:       CoolPotOS\n");
    printk("  .@@@@@@@@*  \033[36m:+@@@@@@@:\033[39m         Processor:    %d\n", cpu_num());
    printk("  &@@@@@@    \033[36m:+@@@@@@@@:\033[39m         CPU:          %s\n", cpu.model_name);
    printk("-@@@@@@*     \033[36m&@@@@@@@=:\033[39m@-        %s Device:  %d\n",is_pcie ? "PCIE" : "PCI ", get_pcie_num());
    printk("*@@@@@&      \033[36m&@@@@@@=:\033[39m@@*        Resolution:   %d x %d\n", framebuffer->width,framebuffer->height);
    printk("&@@@@@+      \033[36m&@@@@@=:\033[39m@@@&        Time:         %s\n", get_date_time());
    printk("@@@@@@:      \033[36m#&&&&=:\033[39m@@@@@        Console:      os_terminal\n");
    printk("&@@@@@+           +@@@@@&        Kernel:       %s\n", KERNEL_NAME);
    printk("*@@@@@@           @@@@@@*        Memory Usage: %d%s / %dMB\n", memory, bytes > 10485760 ? "MB" : "KB",(int) (memory_size / 1024 / 1024));
    printk("-@@@@@@*         #@@@@@@:        64-bit operating system, x86-based processor\n");
    printk(" &@@@@@@*.     .#@@@@@@& \n");
    printk("  =@@@@@@@*----*@@@@@@@- \n");
    printk("  .#@@@@@@@@@@@@@@@@@#.    \n");
    printk("    .#@@@@@@@@@@@@@#.    \n");
    printk("      =&@@@@@@@@@&-      \n");
    printk("        -*&@@@&+:        \n");
}

static void print_help() {
    printk("Usage <command|app_path> [argument...]\n");
    printk("help h ?                 Get shell command help.\n");
    printk("shutdown exit            Shutdown os.\n");
    printk("reboot                   Reboot os.\n");
    printk("lspci                    List all PCI devices.\n");
    printk("sysinfo                  Get system information.\n");
    printk("clear                    Clear terminal screen.\n");
    printk("ps                       List all processes info.\n");
    printk("pkill     <pid>          Stop a process.\n");
    printk("cd        <path>         Change shell work path.\n");
    printk("mkdir     <name>         Make a directory to vfs.\n");
    printk("ls        [path]         List all file or directory.\n");
    printk("echo      <message>      Print a message to terminal.\n");
}

void shell_setup(){
    printk("Welcome to CoolPotOS (%s)\n"
           " * SourceCode:     https://github.com/plos-clan/CoolPotOS\n"
           " * Website:        https://github.com/plos-clan\n"
           " System information as of %s \n"
           "  Process:               %d\n"
           "  User login in:         Kernel\n"
           "Copyright 2024 XIAOYI12 (Build by xmake clang)\n", KERNEL_NAME, get_date_time(),
           get_all_task());
    char com[MAX_COMMAND_LEN];
    char *argv[MAX_ARG_NR];
    int argc = -1;
    shell_work_path = malloc(1024);
    memset(shell_work_path,0,1024);
    shell_work_path[0] = '/';
    while (1){
        printk("\033[32mKernel@localhost: \033[34m%s \033[39m$ ",shell_work_path);
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        memset(com_copy, 0, 100);
        strcpy(com_copy, com);
        argc = cmd_parse(com, argv, ' ');

        if (argc == -1) {
            printk("[Shell]: Error: out of arguments buffer\n");
            continue;
        }

        if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            print_help();
        }else if (!strcmp("shutdown", argv[0]) || !strcmp("exit", argv[0]))
            shutdown_os();
        else if (!strcmp("reboot", argv[0]))
            reboot_os();
        else if (!strcmp("clear", argv[0]))
            printk("\033[H\033[2J\033[3J");
        else if (!strcmp("lspci", argv[0]))
            print_all_pcie();
        else if (!strcmp("sysinfo", argv[0]))
            sys_info();
        else if (!strcmp("ps", argv[0]))
            ps();
        else if (!strcmp("pkill", argv[0]))
            pkill(argc,argv);
        else if (!strcmp("cd", argv[0]))
            cd(argc, argv);
        else if (!strcmp("mkdir", argv[0]))
            mkdir(argc, argv);
        else if (!strcmp("ls", argv[0]))
            ls(argc, argv);
        else if(!strcmp("echo",argv[0]))
            echo(argc,argv);
        else if (!strcmp("test", argv[0])){
            for (int i = 0; i < 100; ++i) {
                printk("count: %d\n",i);
            }
        } else
            printk("\033[31mUnknown command '%s'.\033[39m\n",com_copy);
    }
}
