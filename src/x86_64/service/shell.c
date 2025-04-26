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
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "module.h"
#include "pcb.h"
#include "pci.h"
#include "scheduler.h"
#include "smp.h"
#include "sprintf.h"
#include "atom_queue.h"
#include "pl_readline.h"
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
        char bufx[100];
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
        printk("ls: %s: No such file or directory\n", argv[1]);
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

void lspci() {
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

static void sys_info() {
    cpu_t        cpu = get_cpu_info();
    extern MCFG *mcfg;
    char        *pci_type     = mcfg == NULL ? "PCI " : "PCIE";
    uint32_t     bytes        = get_all_memusage();
    int          memory_used  = (bytes > 10485760) ? bytes / 1048576 : bytes / 1024;
    char         unit         = (bytes > 10485760) ? 'M' : 'K';
    int          memory_total = (int)(memory_size / 1048576);

    const char *logo[] = {"           -*&@@@@@@&*-                 ",
                          "       .&@@@@@@@@@@@@@.\033[36m*=&&&&&&#\033[39m        ",
                          "    =@@@@@@@@@@@@@@@:\033[36m+@@@@@@@@@=:\033[39m       ",
                          "  .@@@@@@@@@@@@@#.\033[36m:+@@@@@@@@@@@=:\033[39m       ",
                          " .@@@@@@@@@       \033[36m&@@@@@@@@@@@*\033[39m.@-      ",
                          " @@@@@@@@#        \033[36m&@@@@@@@@@*\033[39m.@@@&      ",
                          "@@@@@@@@:         \033[36m#&&&&&=*.\033[39m@@@@@@@      ",
                          "&@@@@@@@+                 +@@@@@@&      ",
                          "*@@@@@@@@               @@@@@@@@@*      ",
                          " .@@@@@@@@&:           @@@@@@@@@@       ",
                          "   &@@@@@@@@@--___--@@@@@@@@@&=:        ",
                          "     .#@@@@@@@@@@@@@@@@@@@@&:.          ",
                          "        =&@@@@@@@@@@@@@@@@*             ",
                          "           -@@@@@@@&+:                  "};

    const char *info[] = {"-----------------",
                          "Name: CoolPotOS",
                          "Processor: %d",
                          "CPU: %s",
                          "%s Device: %d",
                          "Resolution: %d x %d",
                          "Time: %s",
                          "Terminal: os_terminal",
                          "Kernel: %s",
                          "Memory Usage: %d%cB / %dMB",
                          "64-bit operating system, x86-based processor."};

    // clang-format off
    printk(logo[0]); printk(info[0]); printk("\n");
    printk(logo[1]); printk(info[1]); printk("\n");
    printk(logo[2]); printk(info[2], cpu_num()); printk("\n");
    printk(logo[3]); printk(info[3], cpu.model_name); printk("\n");
    printk(logo[4]); printk(info[4], pci_type, get_pci_num()); printk("\n");
    printk(logo[5]); printk(info[5], framebuffer->width, framebuffer->height); printk("\n");
    printk(logo[6]); printk(info[6], get_date_time()); printk("\n");
    printk(logo[7]); printk(info[7]); printk("\n");
    printk(logo[8]); printk(info[8], KERNEL_NAME); printk("\n");
    printk(logo[9]); printk(info[9], memory_used, unit, memory_total); printk("\n");
    printk(logo[10]); printk(info[10]); printk("\n");
    printk(logo[11]); printk("\n");
    printk(logo[12]); printk("\n");
    printk(logo[13]); printk("\n");
    // clang-format on
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

// char **split_by_space(const char *input, int *count) {
//     char **tokens = (char **)malloc(MAX_ARG_NR * sizeof(char *));
//     if (!tokens) return NULL;

//     int token_index = 0;
//     int i           = 0;
//     int start       = -1;
//     int len         = strlen(input);

//     while (i <= len) {
//         if (input[i] != ' ' && input[i] != '\0') {
//             if (start == -1) start = i;
//         } else {
//             if (start != -1) {
//                 int   token_len = i - start;
//                 char *token     = (char *)malloc(token_len + 1);
//                 if (!token) {
//                     for (int j = 0; j < token_index; j++)
//                         free(tokens[j]);
//                     free(tokens);
//                     return NULL;
//                 }

//                 memcpy(token, &input[start], token_len);
//                 token[token_len]      = '\0';
//                 tokens[token_index++] = token;

//                 if (token_index >= MAX_ARG_NR) break;
//                 start = -1;
//             }
//         }
//         i++;
//     }

//     tokens[token_index] = NULL; // 最后加个 NULL 结束
//     if (count) *count = token_index;
//     return tokens;
// }

// void trim(char *str) {
//     while (*str && isspace((unsigned char)*str)) {
//         str++;
//     }

//     char *end = str + strlen(str) - 1;
//     while (end > str && isspace((unsigned char)*end)) {
//         end--;
//     }

//     *(end + 1) = '\0';
// }

// ====== pl_readline ======
static int cmd_parse(const char *cmd_str, char **argv, char token) // 用uint8_t是因为" "使用8位整数
{
    int arg_idx = 0;

    while (arg_idx < MAX_ARG_NR) {
        argv[arg_idx] = 0;
        arg_idx++;
    }
    char *next = (char *)cmd_str; // 下一个字符
    int   argc = 0;               // 这就是要返回的argc了

    while (*next) { // 循环到结束为止
        while (*next == token)
            next++;            // 多个token就只保留第一个，windows cmd就是这么处理的
        if (*next == 0) break; // 如果跳过完token之后结束了，那就直接退出
        argv[argc] = next;     // 将首指针赋值过去，从这里开始就是当前参数
        while (*next && *next != token)
            next++;      // 跳到下一个token
        if (*next) {     // 如果这里有token字符
            *next++ = 0; // 将当前token字符设为0（结束符），next后移一个
        }
        if (argc > MAX_ARG_NR) return -1; // 参数太多，超过上限了
        argc++; // argc增一，如果最后一个字符是空格时不提前退出，argc会错误地被多加1
    }
    return argc;
}

typedef struct builtin_cmd {
    const char *name;
    void (*func)(int, char **);
} builtin_cmd_t;

builtin_cmd_t builtin_cmds[] = {
    {"help",     (void (*)(int, char **))print_help },
    {"shutdown", (void (*)(int, char **))cp_shutdown},
    {"exit",     (void (*)(int, char **))cp_shutdown},
    {"reboot",   (void (*)(int, char **))cp_reset   },
    {"lspci",    (void (*)(int, char **))lspci      },
    {"sysinfo",  (void (*)(int, char **))sys_info   },
    {"ps",       (void (*)(int, char **))ps         },
    {"pkill",    (void (*)(int, char **))pkill      },
    {"cd",       (void (*)(int, char **))cd         },
    {"mkdir",    (void (*)(int, char **))mkdir      },
    {"ls",       (void (*)(int, char **))ls         },
    {"echo",     (void (*)(int, char **))echo       },
    {"mount",    (void (*)(int, char **))mount      },
    {"lmod",     (void (*)(int, char **))lmod       },
    {"luser",    (void (*)(int, char **))luser      }
};

/* 内建命令数量 */
static const int builtin_cmd_num = sizeof(builtin_cmds) / sizeof(builtin_cmd_t);

/* 在预定义的命令数组中查找给定的命令字符串 */
int find_cmd(uint8_t *cmd) {
    for (int i = 0; i < builtin_cmd_num; ++i) {
        if (strcmp((const char *)cmd, builtin_cmds[i].name) == 0) { return i; }
    }
    return -1;
}

static int plreadln_getch(void) {
    char ch;

    // temporary alternative to handle unsupported keys
    extern atom_queue *temp_keyboard_buffer;
    while ((ch = atom_pop(temp_keyboard_buffer)) == -1) {
        __asm__ volatile("pause");
    }

    if (ch == 0x0d) { return PL_READLINE_KEY_ENTER; }
    if (ch == 0x7f) { return PL_READLINE_KEY_BACKSPACE; }
    if (ch == 0x9) { return PL_READLINE_KEY_TAB; }
    if (ch == 0x1b) {
        ch = plreadln_getch();
        if (ch == '[') {
            ch = plreadln_getch();
            switch (ch) {
            case 'A': return PL_READLINE_KEY_UP;
            case 'B': return PL_READLINE_KEY_DOWN;
            case 'C': return PL_READLINE_KEY_RIGHT;
            case 'D': return PL_READLINE_KEY_LEFT;
            case 'H': return PL_READLINE_KEY_HOME;
            case 'F': return PL_READLINE_KEY_END;
            case '5':
                if (plreadln_getch() == '~') return PL_READLINE_KEY_PAGE_UP;
                break;
            case '6':
                if (plreadln_getch() == '~') return PL_READLINE_KEY_PAGE_DOWN;
                break;
            default: return -1;
            }
        }
    }
    return ch;
}

static void plreadln_putch(int ch) {
    extern atom_queue *output_buffer;
    atom_push(output_buffer, ch);
}

static void handle_tab(char *buf, pl_readline_words_t words) {
    for (int i = 0; i < builtin_cmd_num; ++i) {
        pl_readline_word_maker_add((char *)builtin_cmds[i].name, words, 1, ' ');
    }
}

static void plreadln_flush(void) {
    /* Nothing */
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

    char     prompt[128];
    uint8_t *argv[MAX_ARG_NR];

    shell_work_path = malloc(1024);
    not_null_assets(shell_work_path, "work path null");
    memset(shell_work_path, 0, 1024);
    shell_work_path[0] = '/';

    pl_readline_t pl = pl_readline_init(plreadln_getch, plreadln_putch, plreadln_flush, handle_tab);

    while (1) {
        sprintf(prompt, "\033[32m%s\033[0m@\033[32mlocalhost: \033[34m%s>\033[0m ",
                tcb->parent_group->user->name, shell_work_path);
        const char *cmd = pl_readline(pl, prompt);

        if (cmd[0] == 0) continue;
        int argc = cmd_parse(cmd, (char **)argv, ' ');

        if (argc == -1) {
            kerror("shell: number of arguments exceed MAX_ARG_NR(30)");
            continue;
        } else if (argc == 0) {
            continue;
        }

        int cmd_index = find_cmd(argv[0]);
        if (cmd_index < 0) {
            printk("Command not found.\n\n");
        } else {
            builtin_cmds[cmd_index].func(argc, (char **)argv);
        }
    }

    pl_readline_uninit(pl);

    // char *line      = malloc(MAX_COMMAND_LEN);
    // shell_work_path = malloc(1024);
    // not_null_assets(shell_work_path, "work path null");
    // memset(shell_work_path, 0, 1024);
    // shell_work_path[0] = '/';

    // infinite_loop {
    //     printk("\033[32m%s\033[0m@\033[32mlocalhost: \033[34m%s> ", tcb->parent_group->user->name,
    //            shell_work_path);
    //     if (gets(line, MAX_COMMAND_LEN) <= 0) continue;
    //     memset(com_copy, 0, 100);
    //     strcpy(com_copy, line);
    //     trim(line);
    //     int    argc;
    //     char **argv = split_by_space(line, &argc);
    //     if (argc == 0) {
    //         free(argv);
    //         continue;
    //     }

    //     if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
    //         print_help();
    //     } else if (!strcmp("shutdown", argv[0]) || !strcmp("exit", argv[0]))
    //         cp_shutdown();
    //     else if (!strcmp("reboot", argv[0]))
    //         cp_reset();
    //     else if (!strcmp("clear", argv[0]))
    //         printk("\033[H\033[2J\033[3J");
    //     else if (!strcmp("lspci", argv[0]))
    //         lspci();
    //     else if (!strcmp("sysinfo", argv[0]))
    //         sys_info();
    //     else if (!strcmp("ps", argv[0]))
    //         ps(argc, argv);
    //     else if (!strcmp("pkill", argv[0]))
    //         pkill(argc, argv);
    //     else if (!strcmp("cd", argv[0]))
    //         cd(argc, argv);
    //     else if (!strcmp("mkdir", argv[0]))
    //         mkdir(argc, argv);
    //     else if (!strcmp("ls", argv[0]))
    //         ls(argc, argv);
    //     else if (!strcmp("echo", argv[0]))
    //         echo(argc, argv);
    //     else if (!strcmp("mount", argv[0]))
    //         mount(argc, argv);
    //     else if (!strcmp("lmod", argv[0]))
    //         lmod(argc, argv);
    //     else if (!strcmp("luser", argv[0]))
    //         luser(argc, argv);
    //     else if (!strcmp("test", argv[0])) {
    //         for (int i = 0; i < 100; ++i) {
    //             printk("count: %d\n", i);
    //         }
    //     } else {
    //         kerror("\033[31mshell: command `%s' not found\033[39m\n", com_copy);
    //     }
    //     for (int i = 0; i < argc; i++) {
    //         free(argv[i]);
    //     }
    //     free(argv);
    // }
}
