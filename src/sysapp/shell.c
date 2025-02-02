#include "shell.h"
#include "limine.h"
#include "kprint.h"
#include "krlibc.h"
#include "timer.h"
#include "scheduler.h"
#include "keyboard.h"
#include "pci.h"
#include "cpuid.h"
#include "gop.h"
#include "heap.h"
#include "pcb.h"

extern void cp_shutdown();
extern void cp_reset();

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
    printk("PID  %-*s       RAM(byte)  Level   Priority  Time\n", longest_name_len, "NAME");
    while (pcb != NULL) {
        printk("%-5d%-*s%10d        %-8s%-10d%-d\n", pcb->pid, longest_name_len, pcb->name,
               pcb->mem_usage,
               pcb->task_level == TASK_KERNEL_LEVEL ? "Kernel" : pcb->task_level == TASK_SYSTEM_SERVICE_LEVEL ? "System"
                                                                                                              : "User",
               pcb->task_level, pcb->cpu_clock);
        pcb = pcb->next;
        if(pcb->pid == kernel_head_task->pid) break;
    }
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

static void sys_info() {
    cpu_t cpu = get_cpu_info();

    uint32_t bytes = get_all_memusage();
    int memory = (bytes > 10485760) ? bytes / 1048576 : bytes / 1024;

    printk("        -*&@@@&*-        \n");
    printk("      =&@@@@@@@@@:\033[36m-----\033[39m          -----------------\n");
    printk("    .&@@@@@@@@@@:\033[36m+@@@@@:\033[39m         OSName:       CoolPotOS\n");
    printk("  .@@@@@@@@@@@:\033[36m+@@@@@@@:\033[39m         Process:      %d\n", get_all_task());
    printk("  &@@@@@@@@@@:\033[36m+@@@@@@@@:\033[39m         CPU:          %s\n", cpu.model_name);
    printk("-@@@@@@*     \033[36m&@@@@@@@=:\033[39m@-        PCI Device:   %d\n", get_pci_num());
    printk("*@@@@@&      \033[36m&@@@@@@=:\033[39m@@*        Resolution:   %d x %d\n", framebuffer->width,
           framebuffer->height);
    printk("&@@@@@+      \033[36m&@@@@@=:\033[39m@@@&        Time:         %s\n", get_date_time());
    printk("@@@@@@:      \033[36m#&&&&=:\033[39m@@@@@        Console:      os_terminal\n");
    printk("&@@@@@+           +@@@@@&        Kernel:       %s\n", KERNEL_NAME);
    printk("*@@@@@@           @@@@@@*        Memory Usage: %d%s / %dMB\n", memory, bytes > 10485760 ? "MB" : "KB",(int) (memory_size / 1024 / 1024));
    printk("-@@@@@@*         #@@@@@@:        64-bit operating system, x86-based processor\n");
    printk(" &@@@@@@*.     .#@@@@@@& \n");
    printk(" =@@@@@@@@*---*@@@@@@@@- \n");
    printk("  .@@@@@@@@@@@@@@@@@@&.  \n");
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
    printk("sysinfo                  Get os system information.\n");
    printk("clear                    Clear terminal screen.\n");
    printk("ps                       List all processes info.\n");
    printk("pkill     <pid>          Stop a process.\n");
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
    while (1){
        printk("\033[32mKernel@localhost: \033[34m%s \033[39m$ ","/");
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
            print_all_pci();
        else if (!strcmp("sysinfo", argv[0]))
            sys_info();
        else if (!strcmp("ps", argv[0]))
            ps();
        else if (!strcmp("pkill", argv[0]))
            pkill(argc,argv);
        else if (!strcmp("test", argv[0])){
            for (int i = 0; i < 100; ++i) {
                printk("count: %d\n",i);
            }
        }
    }
}
