/**
 * Feature Shell version 1.0.0beta
 * By linzhichen114
 * GOD BLESS - NOT CRASH
 */

#include "shell.h"
#include "cpuid.h"
#include "cpusp.h"
#include "dlinker.h"
#include "elf_util.h"
#include "gop.h"
#include "heap.h"
#include "keyboard.h"
#include "kprint.h"
#include "krlibc.h"
#include "module.h"
#include "pcb.h"
#include "pci.h"
#include "scheduler.h"
#include "smp.h"
#include "sprintf.h"
#include "timer.h"
#include "vfs.h"

extern void cp_shutdown();

extern void cp_reset();

extern lock_queue *pgb_queue;

char           *shell_work_path;
extern uint64_t memory_size; // hhdm.c
char            com_copy[100];

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
    int  index = 0;
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
        if (index >= buf_size) {
            kernel_error("\nFATAL: OUT OF INPUT BOUNDS\n", (uint64_t)"FATAL ERROR", NULL);
            break;
        }
    }
    buf[index] = '\0';
    printk("%c", c);
    return index;
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
    char  bufx[100];
    if (buf_h[0] != '/') {
        if (!strcmp(shell_work_path, "/"))
            sprintf(bufx, "/%s", buf_h);
        else
            sprintf(bufx, "%s/%s", shell_work_path, buf_h);
    } else
        sprintf(bufx, "%s", buf_h);
    if (vfs_mkdir(bufx) == -1) { printk("Failed create directory [%s].\n", argv[1]); }
}

void ps(int argc, char **argv) {
    size_t longest_name_len = 0;
    if (argc == 1) {
        queue_foreach(pgb_queue, thread) {
            pcb_t longest_name = (pcb_t)thread->data;
            if (strlen(longest_name->name) > longest_name_len)
                longest_name_len = strlen(longest_name->name);
        }
        printk("GID  %-*s  TaskNum Status  User\n", longest_name_len, "NAME");
        queue_foreach(pgb_queue, thread) {
            pcb_t pgb = (pcb_t)thread->data;
            printk("%-5d%-*s  %-7d %s%s\n", pgb->pgb_id, longest_name_len, pgb->name,
                   pgb->pcb_queue->size,
                   pgb->status == RUNNING ? "Running "
                   : pgb->status == START ? "Start   "
                   : pgb->status == WAIT  ? "Wait    "
                                          : "Death   ",
                   pgb->user->name);
        }
    } else if (strcmp(argv[1], "pcb") == 0) {
        uint32_t     bytes           = get_all_memusage();
        size_t       longest_pgb_len = 0;
        uint64_t     all_time        = 0;
        uint64_t     mem_use         = 0;
        int          memory          = (bytes > 10485760) ? bytes / 1048576 : bytes / 1024;
        extern tcb_t kernel_head_task;
        uint64_t     idle_time = kernel_head_task->cpu_timer;
        for (size_t i = 0; i < MAX_CPU; i++) {
            smp_cpu_t cpu = smp_cpus[i];
            if (cpu.ready == 1) {
                queue_foreach(cpu.scheduler_queue, queue) {
                    tcb_t longest_name = (tcb_t)queue->data;
                    if (strlen(longest_name->name) > longest_name_len)
                        longest_name_len = strlen(longest_name->name);
                    if (strlen(longest_name->parent_group->name) > longest_pgb_len)
                        longest_pgb_len = strlen(longest_name->parent_group->name);
                }
            }
        }
        printk("PID  %-*s %-*s  RAM(byte)  Priority  Timer     Status  ProcessorID\n",
               longest_name_len, "NAME", longest_pgb_len, "GROUP");
        for (size_t i = 0; i < MAX_CPU; i++) {
            smp_cpu_t cpu = smp_cpus[i];
            if (cpu.ready == 1) {
                queue_foreach(cpu.scheduler_queue, queue) {
                    tcb_t pcb  = (tcb_t)queue->data;
                    all_time  += pcb->cpu_timer;
                    if (pcb->task_level != TASK_KERNEL_LEVEL) mem_use += pcb->mem_usage;
                    printk("%-5d%-*s %-*s  %-10d %-10d%-10d%sCPU%-d\n", pcb->pid, longest_name_len,
                           pcb->name, longest_pgb_len, pcb->parent_group->name, pcb->mem_usage,
                           pcb->task_level, pcb->cpu_clock,
                           pcb->status == RUNNING ? "Running "
                           : pcb->status == START ? "Start   "
                           : pcb->status == WAIT  ? "Wait    "
                           : pcb->status == DEATH ? "Death   "
                                                  : "Out     ",
                           pcb->cpu_id);
                }
            }
        }
        printk("   --- CPU Usage: %d%% IPS: %d | Memory Usage: %d%s/%dMB ---\n",
               idle_time * 100 / all_time, get_cpu_speed(), mem_use + memory,
               bytes > 10485760 ? "MB" : "KB", memory_size / 1024 / 1024);
    } else
        printk("Unknown argument, please entry 'help'.\n");
}

static void ls(int argc, char **argv) {
    vfs_node_t p;
    if (argc == 1) {
        p = vfs_open(shell_work_path);
    } else {
        char *buf_h = com_copy + 3;
        char  bufx[100];
        if (buf_h[0] != '/') {
            if (!strcmp(shell_work_path, "/"))
                sprintf(bufx, "/%s", buf_h);
            else
                sprintf(bufx, "%s/%s", shell_work_path, buf_h);
        } else
            sprintf(bufx, "%s", buf_h);
        p = vfs_open(bufx);
    }
    if (p == NULL) {
        printk("Cannot fount directory.\n");
        return;
    }
    list_foreach(p->child, i) {
        vfs_node_t c = (vfs_node_t)i->data;
        printk("%s ", c->name);
    }
    printk("\n");
}

static void pkill(int argc, char **argv) {
    if (argc == 1) {
        printk("[Shell-PKILL]: If there are too few parameters.\n");
        return;
    }
    int   pid = strtol(argv[1], NULL, 10);
    pcb_t pcb = found_pcb(pid);
    if (pcb == NULL) {
        printk("Cannot find procces [%d]\n", pid);
        return;
    }
    kill_proc(pcb);
    kinfo("Kill process (%d).", pid);
}

void print_all_pci() {
    printk("Bus:Slot:Func\t[Vendor:Device]\tClass Code\tName\n");
    for (size_t i = 0; i < get_pci_num(); i++) {
        pci_device_t *device = pci_devices[i];
        printk("%03d:%02d:%02d\t[0x%04X:0x%04X]\t<0x%08x>\t%s\n", device->bus, device->slot,
               device->func, device->vendor_id, device->device_id, device->class_code,
               device->name);
    }
}

static void echo(int argc, char **argv) {
    if (argc == 1) {
        printk("[Shell-ECHO]: If there are too few parameters.\n");
        return;
    }
    vfs_node_t stdout = vfs_open("/dev/stdout");
    if (stdout == NULL)
        printk("ERROR: stream device is null.\n");
    else {
        char *buf = com_copy + 5;
        if (vfs_write(stdout, buf, 0, strlen(buf)) == VFS_STATUS_FAILED)
            printk("stdout stream has error.\n");
        vfs_close(stdout);
        printk("\n");
    }
}

static void mount(int argc, char **argv) {
    if (argc < 3) {
        printk("[Shell-MOUNT]: If there are too few parameters.\n");
        return;
    }
    vfs_node_t p = vfs_open(argv[1]);
    if (p == NULL) {
        printk("Cannot found mount directory.\n");
        return;
    }
    if (vfs_mount(argv[2], p) == -1) { printk("Failed mount device [%s]\n", argv[2]); }
}

static void lmod(int argc, char **argv) {
    if (argc == 1) {
        printk("[Shell-LMOD]: If there are too few parameters.\n");
        return;
    }

    extern cp_module_t module_ls[256];
    extern int         module_count;

    if (!strcmp(argv[1], "list")) {
        printk("Model(Kernel) load in /sys/cpkrnl.elf\n");
        for (int i = 0; i < module_count; i++) {
            printk("Model(%s) load in %s\n", module_ls[i].module_name, module_ls[i].path);
        }
    } else {
        if (!strcmp(argv[1], "Kernel")) return;
        cp_module_t *module = get_module(argv[1]);
        if (module == NULL) {
            printk("Cannot find module [%s]\n", argv[1]);
            return;
        }
        dlinker_load(module);
    }
}

static void luser(int argc, char **argv) {
    if (argc == 1) {
        printk("[Shell-LMOD]: If there are too few parameters.\n");
        return;
    }

    cp_module_t *module = get_module(argv[1]);
    if (module == NULL) {
        printk("Cannot find module [%s]\n", argv[1]);
        return;
    }

    page_directory_t *up   = clone_directory(get_kernel_pagedir());
    void             *main = load_executor_elf(module, up);
    if (main == NULL) {
        kwarn("Cannot load elf file.");
        return;
    }
    pcb_t user_task =
        create_process_group(module->module_name, up, get_current_task()->parent_group->user);
    create_user_thread(main, "main", user_task);
    logkf("User application %s : %d : index: %d loaded.\n", module->module_name, user_task->pgb_id,
          user_task->queue_index);
}

static void sys_info1() {
    cpu_t        cpu = get_cpu_info();
    extern MCFG *mcfg;
    uint32_t     bytes  = get_all_memusage();
    int          memory = (bytes > 10485760) ? bytes / 1048576 : bytes / 1024;

    printk("        -*&@@@&*-        \n");
    printk("      =&@@@@@@@@@:\033[36m-----\033[39m          -----------------\n");
    printk("    .&@@@@@@@@@@:\033[36m+@@@@@:\033[39m         Name:         CoolPotOS\n");
    printk("  .@@@@@@@@*  \033[36m:+@@@@@@@:\033[39m         Processor:    %d\n", cpu_num());
    printk("  &@@@@@@    \033[36m:+@@@@@@@@:\033[39m         CPU:          %s\n", cpu.model_name);
    printk("-@@@@@@*     \033[36m&@@@@@@@=:\033[39m@-        %s Device:  %d\n",
           mcfg == NULL ? "PCI " : "PCIE", get_pci_num());
    printk("*@@@@@&      \033[36m&@@@@@@=:\033[39m@@*        Resolution:   %d x %d\n",
           framebuffer->width, framebuffer->height);
    printk("&@@@@@+      \033[36m&@@@@@=:\033[39m@@@&        Time:         %s\n", get_date_time());
    printk("@@@@@@:      \033[36m#&&&&=:\033[39m@@@@@        Terminal:     os_terminal\n");
    printk("&@@@@@+           +@@@@@&        Kernel:       %s\n", KERNEL_NAME);
    printk("*@@@@@@           @@@@@@*        Memory Usage:  %d%s / %dMB\n", memory,
           bytes > 10485760 ? "MB" : "KB", (int)(memory_size / 1024 / 1024));
    printk("-@@@@@@*         #@@@@@@:        64-bit operating system, x86-based processor.\n");
    printk(" &@@@@@@*.     .#@@@@@@&         \033[40m    \033[41m    \033[42m    \033[43m    "
           "\033[44m    \033[45m    \033[46m    \033[47m    \033[49m    \033[0m\n");
    printk("  =@@@@@@@*----*@@@@@@@-         \n");
    printk("  .#@@@@@@@@@@@@@@@@@#.          \n");
    printk("    .#@@@@@@@@@@@@@#.    \n");
    printk("      =&@@@@@@@@@&-      \n");
    printk("        -*&@@@&+:        \n");
}

static void sys_info() {
    cpu_t        cpu = get_cpu_info();
    extern MCFG *mcfg;
    uint32_t     bytes = get_all_memusage();
    int          memory = (bytes > 10485760) ? bytes / 1048576 : bytes / 1024;
    char         unit = (bytes > 10485760) ? 'M' : 'K';

    const char *logo[] = {
            "          -*&@@@@@&*-",
            "       =&@@@@@@@@@@@:\033[36m-----\033[39m           -----------------",
            "     .&@@@@@@@@@@@@:\033[36m+@@@@@:\033[39m          Name:         CoolPotOS",
            "   .@@@@@@@@@@*     \033[36m:+@@@@@@@:\033[39m       Processor:    %d",
            "  &@@@@@@@@         \033[36m:+@@@@@@@@:\033[39m        CPU:          %s",
            "-@@@@@@@@*          \033[36m&@@@@@@@=:\033[39m@-       %s Device:  %d",
            "*@@@@@@@&           \033[36m&@@@@@@=:\033[39m@@*       Resolution:   %d x %d",
            "&@@@@@@@+           \033[36m&@@@@@=:\033[39m@@@&       Time:         %s",
            "@@@@@@@@:           \033[36m#&&&&=:\033[39m@@@@@       Terminal:     os_terminal",
            "&@@@@@@@+            +@@@@@@@&        Kernel:       %s",
            "*@@@@@@@@            @@@@@@@@*        Memory Usage:  %d%cB / %dMB",
            "-@@@@@@@@*          #@@@@@@@@:        64-bit operating system, x86-based processor.",
            " &@@@@@@@@*.      .#@@@@@@@@&         \033[40m    \033[41m    \033[42m    \033[43m    \033[44m    \033[45m    \033[46m    \033[47m    \033[49m    \033[0m",
            "  =@@@@@@@@@*----*@@@@@@@@@-",
            "   .#@@@@@@@@@@@@@@@@@@@#.",
            "     .#@@@@@@@@@@@@@@@#. ",
            "       =&@@@@@@@@@@@&-",
            "          -*&@@@@@&+:"
    };

    printk("%s\n", logo[0]);
    printk("%s\n", logo[1]);
    printk("%s\n", logo[2]);
    printk(logo[3], cpu_num()); printk("\n");
    printk(logo[4], cpu.model_name); printk("\n");
    printk(logo[5], mcfg == NULL ? "PCI " : "PCIE", get_pci_num()); printk("\n");
    printk(logo[6], framebuffer->width, framebuffer->height); printk("\n");
    printk(logo[7], get_date_time()); printk("\n");
    printk("%s\n", logo[8]);
    printk(logo[9], KERNEL_NAME); printk("\n");
    printk(logo[10], memory, unit, (int)(memory_size / 1024 / 1024)); printk("\n");

    for (int i = 11; i < 18; i++) {
        printk("%s\n", logo[i]);
    }
}

static void sys_info3() {
    cpu_t        cpu = get_cpu_info();
    extern MCFG *mcfg;
    uint32_t     bytes = get_all_memusage();
    int          memory = (bytes > 10485760) ? bytes / 1048576 : bytes / 1024;
    char         unit = (bytes > 10485760) ? 'M' : 'K';

    const char *logo[] = {
            "          -*&@@@@@&*-          ",
            "       =&@@@@@@@@@@@@@=\033[36m-----\033[39m    -----------------",
            "     .&@@@@@@@@@@@@@@\033[36m+@@@@@:\033[39m    Name:         CoolPotOS",
            "   .@@@@@@@@@*  \033[36m:+@@@@@@@@@.\033[39m    Processor:    %d",
            "  &@@@@@@@@*    \033[36m:+@@@@@@@@@@&\033[39m   CPU:          %s",
            " -@@@@@@@@       \033[36m&@@@@@@@@@-\033[39m    %s Device:  %d",
            "*@@@@@@@&        \033[36m&@@@@@@@@@@*\033[39m   Resolution:   %d x %d",
            "&@@@@@@@+        \033[36m&@@@@@@@@@&\033[39m    Time:         %s",
            "@@@@@@@@@        \033[36m#&&&&@@@@@@\033[39m    Terminal:     os_terminal",
            "&@@@@@@@+           +@@@@@@@&   Kernel:       %s",
            "*@@@@@@@&           &@@@@@@@*   Memory Usage:  %d%cB / %dMB",
            " -@@@@@@@@         @@@@@@@@-    64-bit operating system, x86-based processor.",
            "  &@@@@@@@@*.     *@@@@@@@@&    \033[40m    \033[41m    \033[42m    \033[43m    \033[44m    \033[45m    \033[46m    \033[47m    \033[49m    \033[0m",
            "   .@@@@@@@@@**@@@@@@@@@@@.     ",
            "     .&@@@@@@@@@@@@@@@@&.       ",
            "       =&@@@@@@@@@@@@@=         ",
            "          -*&@@@@@&*-           "
    };

    printk("%s\n", logo[0]);
    printk("%s\n", logo[1]);
    printk("%s\n", logo[2]);
    printk(logo[3], cpu_num()); printk("\n");
    printk(logo[4], cpu.model_name); printk("\n");
    printk(logo[5], mcfg == NULL ? "PCI " : "PCIE", get_pci_num()); printk("\n");
    printk(logo[6], framebuffer->width, framebuffer->height); printk("\n");
    printk(logo[7], get_date_time()); printk("\n");
    printk("%s\n", logo[8]);
    printk(logo[9], KERNEL_NAME); printk("\n");
    printk(logo[10], memory, unit, (int)(memory_size / 1024 / 1024)); printk("\n");

    for (int i = 11; i < 17; i++) {
        printk("%s\n", logo[i]);
    }
}

static void print_help() {
    printk("Usage <command|app_path> [argument...]\n");
    printk("help h ?                 Get shell help info.\n");
    printk("shutdown exit            Shutdown kernel.\n");
    printk("reboot                   Restart kernel.\n");
    printk("lspci/lspcie             List all PCI/PCIE devices.\n");
    printk("sysinfo                  Get system info.\n");
    printk("clear                    Clear screen.\n");
    printk("ps        [pcb]          List task group or all running tasks.\n");
    printk("pkill     <pid>          Kill a tasks.\n");
    printk("cd        <path>         Change shell work path.\n");
    printk("mkdir     <name>         Create a directory.\n");
    printk("ls        [path]         List all file or directory.\n");
    printk("echo      <message>      Print a message.\n");
    printk("mount     <path> <dev>   Mount a device to path.\n");
    printk("lmod      <module|list>  Load or list model.\n");
    printk("luser     <module>       Load a user application.\n");
}

char **split_by_space(const char *input, int *count) {
    char **tokens = (char **)malloc(MAX_ARG_NR * sizeof(char *));
    if (!tokens) return NULL;

    int token_index = 0;
    int i           = 0;
    int start       = -1;
    int len         = strlen(input);

    while (i <= len) {
        if (input[i] != ' ' && input[i] != '\0') {
            if (start == -1) start = i;
        } else {
            if (start != -1) {
                int   token_len = i - start;
                char *token     = (char *)malloc(token_len + 1);
                if (!token) {
                    for (int j = 0; j < token_index; j++)
                        free(tokens[j]);
                    free(tokens);
                    return NULL;
                }

                memcpy(token, &input[start], token_len);
                token[token_len]      = '\0';
                tokens[token_index++] = token;

                if (token_index >= MAX_ARG_NR) break;
                start = -1;
            }
        }
        i++;
    }

    tokens[token_index] = NULL; // 最后加个 NULL 结束
    if (count) *count = token_index;
    return tokens;
}

void trim(char *str) {
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    *(end + 1) = '\0';
}

_Noreturn void shell_setup() {
    printk("Welcome to CoolPotOS (%s)!\n"
           " * SourceCode:        https://github.com/plos-clan/CoolPotOS\n"
           " * Website:           https://github.com/plos-clan\n"
           " System information as of %s \n"
           "  Tasks:              %d\n"
           "  Logged:             %s\n"
           "MIT License 2024-2025 plos-clan\n",
           KERNEL_NAME, get_date_time(), get_all_task(), tcb->parent_group->user->name);
    char *line      = malloc(MAX_COMMAND_LEN);
    shell_work_path = malloc(1024);
    not_null_assets(shell_work_path, "work path null");
    memset(shell_work_path, 0, 1024);
    shell_work_path[0] = '/';

    infinite_loop {
        printk("\033[32m%s\033[0m@\033[32mlocalhost: \033[34m%s> ", tcb->parent_group->user->name,
               shell_work_path);
        if (gets(line, MAX_COMMAND_LEN) <= 0) continue;
        memset(com_copy, 0, 100);
        strcpy(com_copy, line);
        trim(line);
        int    argc;
        char **argv = split_by_space(line, &argc);
        if (argc == 0) {
            free(argv);
            continue;
        }

        if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            print_help();
        } else if (!strcmp("shutdown", argv[0]) || !strcmp("exit", argv[0]))
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
            ps(argc, argv);
        else if (!strcmp("pkill", argv[0]))
            pkill(argc, argv);
        else if (!strcmp("cd", argv[0]))
            cd(argc, argv);
        else if (!strcmp("mkdir", argv[0]))
            mkdir(argc, argv);
        else if (!strcmp("ls", argv[0]))
            ls(argc, argv);
        else if (!strcmp("echo", argv[0]))
            echo(argc, argv);
        else if (!strcmp("mount", argv[0]))
            mount(argc, argv);
        else if (!strcmp("lmod", argv[0]))
            lmod(argc, argv);
        else if (!strcmp("luser", argv[0]))
            luser(argc, argv);
        else if (!strcmp("test", argv[0])) {
            for (int i = 0; i < 100; ++i) {
                printk("count: %d\n", i);
            }
        } else {
            kerror("\033[31mshell: command `%s' not found\033[39m\n", com_copy);
        }
        for (int i = 0; i < argc; i++) {
            free(argv[i]);
        }
        free(argv);
    }
}
