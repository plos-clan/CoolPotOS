#include "shell.h"
#include "atom_queue.h"
#include "cpuid.h"
#include "dlinker.h"
#include "elf_util.h"
#include "gop.h"
#include "heap.h"
#include "keyboard.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "memstats.h"
#include "module.h"
#include "os_terminal.h"
#include "pcb.h"
#include "pci.h"
#include "pl_readline.h"
#include "scheduler.h"
#include "smp.h"
#include "sprintf.h"
#include "timer.h"
#include "vfs.h"

extern void cp_shutdown();
extern void cp_reset();

extern lock_queue *pgb_queue;
extern atom_queue *temp_keyboard_buffer;

static pcb_t    shell_process;
extern uint64_t memory_size; // hhdm.c

static inline int isprint_syshell(int c) {
    return (c > 0x1F && c < 0x7F);
}

static void cd(int argc, char **argv) {
    if (argc == 1) {
        printk("cd: Too few parameters.\n");
        return;
    }
    char *s = argv[1];
    char *path;
    if (s[0] == '/') {
        path = strdup(s);
    } else {
        path = pathacat(shell_process->cwd, s);
    }

    char *normalized_path = normalize_path(path);
    free(path);

    if (normalized_path == NULL) {
        printk("cd: Memory allocation error\n");
        return;
    }

    vfs_node_t node;
    if ((node = vfs_open(normalized_path)) == NULL) {
        printk("cd: %s: No such file or directory\n", s);
        free(normalized_path);
        return;
    }

    if (node->type == file_dir) {
        strcpy(shell_process->cwd, normalized_path);
    } else {
        printk("cd: %s: Not a directory\n", s);
    }

    free(normalized_path);
}

static void mkdir(int argc, char **argv) {
    if (argc == 1) {
        printk("mkdir: Too few parameters.\n");
        return;
    }
    char *path;
    if (argv[1][0] == '/') {
        path = strdup(argv[1]);
    } else {
        path = pathacat(shell_process->cwd, argv[1]);
    }
    if (vfs_mkdir(path) == -1) { printk("mkdir: Failed create directory [%s].\n", path); }
    free(path);
}

static void mkfile(int argc, char **argv) {
    if (argc == 1) {
        printk("mkfile: Too few parameters.\n");
        return;
    }
    char *path;
    if (argv[1][0] == '/') {
        path = strdup(argv[1]);
    } else {
        path = pathacat(shell_process->cwd, argv[1]);
    }
    if (vfs_mkfile(path) == -1) { printk("mkfile: Failed create file [%s].\n", path); }
    free(path);
}

void ps(int argc, char **argv) {
    size_t longest_name_len = 0;
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printk("\033[1;34mUsage: ps [OPTION]\033[0m\n");
        printk("\033[1;33mDisplay information about active processes.\033[0m\n\n");
        printk("\033[1mOptions:\033[0m\n");
        printk("  \033[32m-v\033[0m         \033[36mShow detailed process information\033[0m\n");
        printk("  \033[32m-h, --help\033[0m \033[36mDisplay this help message\033[0m\n");
        return;
    }
    if (argc == 1) {
        queue_foreach(pgb_queue, thread) {
            pcb_t longest_name = (pcb_t)thread->data;
            if (strlen(longest_name->name) > longest_name_len)
                longest_name_len = strlen(longest_name->name);
        }
        printk("\033[1;34mGID  %-*s  TaskNum Status  User     Cmdline\033[0m\n", longest_name_len,
               "NAME");
        queue_foreach(pgb_queue, thread) {
            pcb_t pgb = (pcb_t)thread->data;
            printk("\033[35m%-5d\033[0m", pgb->pid);
            printk("\033[1;36m%-*s\033[0m  ", longest_name_len, pgb->name);
            printk("\033[32m%-7d\033[0m ", pgb->pcb_queue->size);
            if (pgb->status == RUNNING)
                printk("\033[32mRunning \033[0m");
            else if (pgb->status == START)
                printk("\033[33mStart   \033[0m");
            else if (pgb->status == CREATE)
                printk("\033[33mCreate  \033[0m");
            else if (pgb->status == WAIT)
                printk("\033[36mWait    \033[0m");
            else if (pgb->status == FUTEX)
                printk("\033[36mFutex   \033[0m");
            else
                printk("\033[31mDeath   \033[0m");

            printk("\033[33m%-9s\033[0m", pgb->user->name);

            if (pgb->cmdline && strlen(pgb->cmdline) > 0)
                printk("\033[37m%s\033[0m\n", pgb->cmdline);
            else
                printk("\033[90m<empty>\033[0m\n"); // 没有命令行时的占位
        }
    } else if (strcmp(argv[1], "-v") == 0) {
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
        printk("\033[1;34mPID  %-*s %-*s  RAM(byte)  Priority  Timer     Status  "
               "ProcessorID\033[0m\n",
               longest_name_len, "NAME", longest_pgb_len, "GROUP");
        for (size_t i = 0; i < MAX_CPU; i++) {
            smp_cpu_t cpu = smp_cpus[i];
            if (cpu.ready == 1) {
                queue_foreach(cpu.scheduler_queue, queue) {
                    tcb_t pcb  = (tcb_t)queue->data;
                    all_time  += pcb->cpu_timer;
                    if (pcb->task_level != TASK_KERNEL_LEVEL) mem_use += pcb->mem_usage;

                    printk("\033[35m%-5d\033[0m", pcb->tid);
                    printk("\033[1;36m%-*s\033[0m ", longest_name_len, pcb->name);
                    printk("\033[33m%-*s\033[0m  ", longest_pgb_len, pcb->parent_group->name);
                    printk("\033[32m%-10d\033[0m ", pcb->mem_usage);
                    printk("\033[35m%-10d\033[0m", pcb->task_level);
                    printk("\033[36m%-10d\033[0m", pcb->cpu_clock);
                    if (pcb->status == RUNNING)
                        printk("\033[32mRunning \033[0m");
                    else if (pcb->status == START)
                        printk("\033[33mStart   \033[0m");
                    else if (pcb->status == CREATE)
                        printk("\033[33mCreate  \033[0m");
                    else if (pcb->status == WAIT)
                        printk("\033[36mWait    \033[0m");
                    else if (pcb->status == DEATH)
                        printk("\033[31mDeath   \033[0m");
                    else if (pcb->status == FUTEX)
                        printk("\033[31mFutex   \033[0m");
                    else
                        printk("\033[37mOut     \033[0m");
                    printk("\033[1;34mCPU%-d\033[0m\n", pcb->cpu_id);
                }
            }
        }
        printk("\033[1;32m   --- CPU Usage: %d%% | Memory Usage: %d%s/%dMB ---\033[0m\n",
               idle_time * 100 / all_time, mem_use + memory, bytes > 10485760 ? "MB" : "KB",
               memory_size / 1024 / 1024);
    } else {
        printk("\033[31mUnknown argument, please entry 'help'.\033[0m\n");
    }
}

static void ls(int argc, char **argv) {
    vfs_node_t p;
    if (argc == 1) {
        p = vfs_open(shell_process->cwd);
    } else {
        if (argv[1][0] == '/') {
            p = vfs_open(argv[1]);
        } else {
            char *path = pathacat(shell_process->cwd, argv[1]);
            p          = vfs_open(path);
            free(path);
        }
    }
    if (p == NULL) {
        printk("\033[31mls: %s: No such file or directory\033[0m\n", argv[1]);
        return;
    }
    if (p->type == file_dir) {
        list_foreach(p->child, i) {
            vfs_node_t c = (vfs_node_t)i->data;
            if (c->type == file_dir) {
                printk("\033[1;34m%s\033[0m ", c->name);
            } else {
                printk("%s ", c->name);
            }
        }
        printk("\n");
    } else {
        if (p->type == file_dir) {
            printk("\033[1;34m%s\033[0m\n", p->name);
        } else {
            printk("%s\n", p->name);
        }
    }
}

static void pkill(int argc, char **argv) {
    if (argc == 1) {
        printk("\033[31mpkill: Too few parameters.\033[0m\n");
        return;
    }
    int   pid = strtol(argv[1], NULL, 10);
    pcb_t pcb = found_pcb(pid);
    if (pcb == NULL) {
        printk("\033[31mpkill: Cannot find process [%d]\033[0m\n", pid);
        return;
    }
    kill_proc(pcb, 135, false);
    printk("\033[32mProcess %d killed.\033[0m\n", pid);
}

void lspci() {
    printk("\033[1;34mBus:Slot:Func\t[Vendor:Device]\tClass Code\tName\033[0m\n");
    for (size_t i = 0; i < get_pci_num(); i++) {
        pci_device_t *device = pci_devices[i];
        printk("\033[35m%03d:%02d:%02d\033[0m\t", device->bus, device->slot, device->func);
        printk("[\033[32m0x%04X:0x%04X\033[0m]\t", device->vendor_id, device->device_id);
        printk("<\033[33m0x%08x\033[0m>\t", device->class_code);
        printk("\033[1;36m%s\033[0m\n", device->name);
    }
}

static void echo(int argc, char **argv) {
    if (argc == 1) { return; }
    vfs_node_t stdout = vfs_open("/dev/stdio");
    if (stdout == NULL) {
        printk("\033[31mstdio stream device is null.\033[0m\n");
    } else {
        for (int i = 1; i < argc; ++i) {
            char *buf = argv[i];
            if (vfs_write(stdout, buf, 0, strlen(buf)) == VFS_STATUS_FAILED) {
                printk("\033[31mstdio stream device has error.\033[0m\n");
            }
            char c = ' ';
            vfs_write(stdout, &c, 0, 1);
        }

        vfs_close(stdout);
        printk("\n");
    }
}

static void mount(int argc, char **argv) {
    if (argc < 3) {
        printk("mount: Too few parameters.\n");
        return;
    }
    vfs_node_t p = vfs_open(argv[2]);
    if (p == NULL) {
        printk("Cannot found mount directory.\n");
        return;
    }
    if (vfs_mount(argv[1], p) == -1) { printk("Failed mount device [%s]\n", argv[1]); }
}

static void lmod(int argc, char **argv) {
    if (argc == 1) {
        printk("\033[31mlmod: no module name.\033[0m\n");
        return;
    }

    extern cp_module_t module_ls[256];
    extern int         module_count;

    if (!strcmp(argv[1], "list")) {
        for (int i = 0; i < module_count; i++) {
            printk("\033[1;36mModule(%s)\033[0m load in \033[1;34m%s\033[0m\n",
                   module_ls[i].module_name, module_ls[i].path);
        }
    } else {
        if (!strcmp(argv[1], "Kernel")) return;
        cp_module_t *module = get_module(argv[1]);
        if (module == NULL) {
            printk("\033[31mCannot find module [%s]\033[0m\n", argv[1]);
            return;
        }
        dlinker_load(module);
    }
}

static bool exec(int argc, char **argv) {
    if (argc == 1) {
        printk("\033[31mexec: no module name.\033[0m\n");
        return false;
    }

    vfs_node_t file = vfs_open(argv[1]);
    if (file == NULL) {
        printk("\033[31mCannot find file [%s]\033[0m\n", argv[1]);
        return false;
    }

    uint8_t *data = malloc(file->size);
    not_null_assets(data, "Out of Memory Kernel Heap\n");
    if (vfs_read(file, data, 0, file->size) == (size_t)VFS_STATUS_FAILED) {
        printk("\033[31mFailed to read file [%s]\033[0m\n", file->name);
        free(data);
        return false;
    }

    page_directory_t *up         = clone_page_directory(get_kernel_pagedir(), false);
    uint64_t          load_start = UINT64_MAX;
    void             *main       = load_executor_elf(data, up, 0, &load_start);
    if (main == NULL) {
        printk("\033[31mCannot load elf file.\033[0m\n");
        return false;
    }

    char *name        = file->name;
    ucb_t user_handle = get_current_task()->parent_group->user;

    int total_len = 0;
    for (int i = 1; i < argc; ++i) {
        total_len += strlen(argv[i]) + 1;
    }
    char *result = malloc(total_len);
    memset(result, 0, total_len);
    for (int i = 1; i < argc; ++i) {
        strcat(result, argv[i]);
        if (i != argc - 1) strcat(result, " ");
    }

    pcb_t user_task =
        create_process_group(name, up, user_handle, result, shell_process, data, file->size);
    user_task->load_start = load_start;
    user_task->task_level = TASK_APPLICATION_LEVEL;
    create_user_thread(main, "main", user_task);

    free(result);

    int    pgb_id      = user_task->pid;
    size_t queue_index = user_task->queue_index;
    logkf("User application %s : %d : index: %d loaded.\n", name, pgb_id, queue_index);
    int status;
    get_current_task()->status = WAIT;
    waitpid(user_task->pid, &status);
    get_current_task()->status = RUNNING;
    return true;
}

static void read(int argc, char **argv) {
    if (argc == 1) {
        printk("read: Too few parameters.\n");
        return;
    }
    char *path;
    if (argv[1][0] == '/') {
        path = strdup(argv[1]);
    } else {
        path = pathacat(shell_process->cwd, argv[1]);
    }
    vfs_node_t file = vfs_open(path);
    if (file != NULL) {
        uint8_t *buffer = malloc(file->size);
        if (vfs_read(file, buffer, 0, file->size) != (size_t)VFS_STATUS_FAILED) {
            for (size_t i = 0; i < file->size; i++) {
                printk("%c", buffer[i]);
            }
            printk("\n");
        } else
            printk("Error: cannot open file [%s]\n", path);
        vfs_close(file);
    } else
        printk("Error: cannot open file [%s]\n", path);
    free(path);
}

static void mem() {
    printk("CoolPotOS Memory Statistics\n");
    printk("----------------------------\n");
    printk("All Memory:      %d MB\n", get_all_memory() / 1024 / 1024);
    printk("Free Memory:     %d MB\n", get_available_memory() / 1024 / 1024);
    printk("Used Memory:     %d MB\n", get_used_memory() / 1024 / 1024);
    printk("Commit Memory:   %d MB\n", get_commit_memory() / 1024 / 1024);
    printk("Reserved Memory: %d MB\n", get_reserved_memory() / 1024 / 1024);
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

static void clear() {
    printk("\033[2J\033[1;1H");
}

static void print_help() {
    printk("Usage \033[1m<command|app_path>\033[0m [argument...]\n");
    printk("\033[1mhelp\033[0m                     Get shell help info.\n");
    printk("\033[1mshutdown exit\033[0m            Shutdown kernel.\n");
    printk("\033[1mreboot\033[0m                   Restart kernel.\n");
    printk("\033[1mlspci\033[0m                    List all PCI/PCIE devices.\n");
    printk("\033[1msysinfo\033[0m                  Get system info.\n");
    printk("\033[1mclear\033[0m                    Clear screen.\n");
    printk("\033[1mps\033[0m        [pcb]          List task group or all running tasks.\n");
    printk("\033[1mpkill\033[0m     <pid>          Kill a tasks.\n");
    printk("\033[1mcd\033[0m        <path>         Change shell work path.\n");
    printk("\033[1mmkdir\033[0m     <name>         Create a directory.\n");
    printk("\033[1mls\033[0m        [path]         List all file or directory.\n");
    printk("\033[1mecho\033[0m      <message>      Print a message.\n");
    printk("\033[1mmount\033[0m     <dev> <path>   Mount a device to path.\n");
    printk("\033[1mlmod\033[0m      <module|list>  Load or list module.\n");
    printk("\033[1mexec\033[0m      <module>       Load a user application.\n");
    printk("\033[1mmkfile\033[0m    <path>         Create a file.\n");
    printk("\033[1mread\033[0m      <path>         Read a file info.\n");
    printk("\033[1mmem\033[0m       <path>         List memory statistical information.\n");
}

// ====== pl_readline ======

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
    {"clear",    (void (*)(int, char **))clear      },
    {"ps",       (void (*)(int, char **))ps         },
    {"pkill",    (void (*)(int, char **))pkill      },
    {"cd",       (void (*)(int, char **))cd         },
    {"mkdir",    (void (*)(int, char **))mkdir      },
    {"ls",       (void (*)(int, char **))ls         },
    {"echo",     (void (*)(int, char **))echo       },
    {"mount",    (void (*)(int, char **))mount      },
    {"lmod",     (void (*)(int, char **))lmod       },
    {"exec",     (void (*)(int, char **))exec       },
    {"mkfile",   (void (*)(int, char **))mkfile     },
    {"read",     (void (*)(int, char **))read       },
    {"mem",      (void (*)(int, char **))mem        },
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
    char ch = 0;

    /* temporary alternative to handle unsupported keys */
    ch = kernel_getch();

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

static int plreadln_putch(int ch) {
    tty_t *tty = get_current_task()->parent_group->tty;
    tty->putchar(tty, ch);
    return 0;
}

static char *truncate_path(char *buf, bool clear) {
    char *path = malloc(strlen(buf) + 1);
    memcpy(path, buf, strlen(buf) + 1);
    bool find_slash = false;
    for (isize i = strlen(path); i >= 0; i--) {
        if (path[i] == '/') {
            path[i + 1] = '\0';
            find_slash  = true;
            break;
        }
    }
    if (!find_slash && clear) { path[0] = '\0'; }
    return path;
}

static void add_to_maker(vfs_node_t node, char *word,
                         pl_readline_words_t words) { // 把一个目录或文件添加到自动补全器中
    if (node->type == file_dir) {
        pl_readline_word_maker_add(word, words, false, PL_COLOR_YELLOW, '/');
    } else {
        pl_readline_word_maker_add(word, words, false, PL_COLOR_CYAN, ' ');
    }
}

static void handle_tab(char *buf, pl_readline_words_t words) {
    for (int i = 0; i < builtin_cmd_num; ++i) {
        char *cmd = (char *)builtin_cmds[i].name;
        pl_readline_word_maker_add(cmd, words, true, PL_COLOR_BLUE, ' ');
    }

    if (buf[0] == '\0') {
        vfs_node_t p = vfs_open(shell_process->cwd);
        if (!p) return;
        list_foreach(p->child, i) {
            vfs_node_t c = (vfs_node_t)i->data;
            vfs_update(c);
            add_to_maker(c, c->name, words);
        }
    } else if (buf[0] == '/') {
        char      *path = truncate_path(buf, false);
        vfs_node_t p    = vfs_open(path);
        if (!p) {
            free(path);
            return;
        }
        list_foreach(p->child, i) {
            vfs_node_t c        = (vfs_node_t)i->data;
            char      *new_path = pathacat(path, c->name);
            vfs_update(c);
            add_to_maker(c, new_path, words);
            free(new_path);
        }
        free(path);
    } else {
        char *path      = truncate_path(buf, true);
        char *full_path = pathacat(shell_process->cwd, path);

        vfs_node_t p = vfs_open(full_path);
        if (!p) {
            free(path);
            return;
        }
        list_foreach(p->child, i) {
            vfs_node_t c = (vfs_node_t)i->data;
            vfs_update(c);
            char *new_path = (char *)malloc(strlen(path) + strlen(c->name) + 1);
            sprintf(new_path, "%s%s", path, c->name);
            add_to_maker(c, new_path, words);
            free(new_path);
        }
        free(path);
    }
}

static void plreadln_flush(void) {}

_Noreturn void shell_setup() {
    char *user_name = get_current_task()->parent_group->user->name;

    printk("\033[1mWelcome to CoolPotOS (\033[1;32m%s\033[0m\033[1m)!\033[0m\n"
           " * \033[1mSourceCode:\033[0m        https://github.com/plos-clan/CoolPotOS\n"
           " * \033[1mWebsite:\033[0m           https://cpos.plos-clan.org\n"
           "\033[1mSystem information as of \033[1;36m%s\033[0m \n"
           "  \033[1mTasks:\033[0m              \033[1;33m%d\033[0m\n"
           "  \033[1mLogged:\033[0m             \033[1;35m%s\033[0m\n"
           "\033[1mMIT License 2024-2025 plos-clan\033[0m\n",
           KERNEL_NAME, get_date_time(), get_all_task(), user_name);

    char     prompt[128];
    uint8_t *argv[MAX_ARG_NR];

    shell_process = get_current_task()->parent_group;

    memset(shell_process->cwd, 0, 1024);
    shell_process->cwd[0] = '/';

    pl_readline_t pl = pl_readline_init(plreadln_getch, plreadln_putch, plreadln_flush, handle_tab);

    /***************** debug *******************/
    {
        int   argc    = 3;
        char *argv[3] = {"mount", "/dev/part1", "/"};
        mount(argc, argv);
    }
    {
        int   argc         = 2;
        char *argv_bash[2] = {"exec", "/bin/bash"};
        char *argv_sh[2]   = {"exec", "/bin/sh"};
        exec(argc, argv_bash) || exec(argc, argv_sh);
    }
    /***************** debug *******************/

    loop {
        char *template = "\033[01;32m%s\033[0m@\033[01;32mlocalhost: \033[34m%s>\033[0m ";
        sprintf(prompt, template, user_name, shell_process->cwd);

        const char *cmd = pl_readline(pl, prompt);
        if (cmd[0] == 0) continue;
        int argc = cmd_parse(cmd, (char **)argv, ' ');

        if (argc == -1) {
            kerror("shell: number of arguments exceed MAX_ARG_NR(30)");
            continue;
        }
        if (argc == 0) continue;

        int cmd_index = find_cmd(argv[0]);
        if (cmd_index < 0) {
            printk("Command not found.\n\n");
        } else {
            builtin_cmds[cmd_index].func(argc, (char **)argv);
        }
    }

    pl_readline_uninit(pl);
}
