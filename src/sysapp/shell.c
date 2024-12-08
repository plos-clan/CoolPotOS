#include "shell.h"
#include "krlibc.h"
#include "klog.h"
#include "keyboard.h"
#include "cmos.h"
#include "vfs.h"
#include "pcb.h"
#include "cpuid.h"
#include "pci.h"
#include "video.h"
#include "scheduler.h"
#include "speaker.h"

static inline int isprint_syshell(int c) {
    return (c > 0x1F && c < 0x7F);
}

static char getc() {
    char c;
    do{
        c = kernel_getch();
        if(c == '\b' || c == '\n') break;
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
            printk("%c",c);
        }
    }
    buf[index] = '\0';
    printk("%c",c);
    return index;
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

static void mkdir(int argc,char** argv){
    if (argc == 1) {
        printk("[Shell-MKDIR]: If there are too few parameters.\n");
        return;
    }
    if(vfs_mkdir(argv[1]) == -1){
        printk("Failed create directory [%s].\n",argv[1]);
    }
}

static void mount(int argc,char** argv){
    if (argc == 2) {
        printk("[Shell-MOUNT]: If there are too few parameters.\n");
        return;
    }
    vfs_node_t p = vfs_open(argv[1]);
    if(p == NULL) {
        printk("Cannot found mount directory.\n");
        return;
    }
    if(vfs_mount(argv[2],p) == -1){
        printk("Failed mount device [%s]\n",argv[2]);
    }
}

static void ls(int argc,char** argv){
    vfs_node_t p;
    if (argc == 1) {
        p = vfs_open("/");
    } else{
        p = vfs_open(argv[1]);
    }
    if(p == NULL){
        printk("Cannot fount directory.\n");
        return;
    }
    list_foreach(p->child, i) {
        vfs_node_t c = (vfs_node_t) i->data;
        printk("%s ", c->name);
    }
    printk("\n");
}

static void read(int argc,char** argv) {
    if (argc == 1) {
        printk("[Shell-MKDIR]: If there are too few parameters.\n");
        return;
    }
    vfs_node_t file = vfs_open(argv[1]);
    if(file != NULL){
        char* buf = kmalloc(file->size);
        if(vfs_read(file,buf,0,file->size) == -1){
            goto read_error;
        }
        for (size_t i = 0; i < file->size; i++) {
            printk("%c",buf[i]);
        }
        printk("\n");
        return;
    }
    read_error:
    printk("Cannot read file.\n");
}

static void shutdown_os(){
    shutdown();
}

static void reboot_os(){
    reboot();
}

static inline void foreach(list_t list){
    list_foreach(list,node){
        vfs_node_t c = (vfs_node_t)node->data;
        if(c->type == file_dir) foreach(c->child);
        else{
            char* buf = kmalloc(c->size);
            vfs_read(c,buf,0,c->size);
            printk("%s > %s\n",c->name);
        }
    }
}

// 实现ps命令
static void ps(){
    extern pcb_t *running_proc_head;
    // 找出最长进程名
    pcb_t *longest_name = running_proc_head;
    size_t longest_name_len = 0;
    while(longest_name != NULL){
        if(strlen(longest_name->name) > longest_name_len)
            longest_name_len = strlen(longest_name->name);
        longest_name = longest_name->next;
    }
    pcb_t *pcb = running_proc_head;
    printk("PID  %-*s       RAM(byte)  Level   Priority  Time\n",longest_name_len,"NAME");
    while(pcb != NULL){
        printk("%-5d%-*s%10d        %-8s%-10d%-d\n",pcb->pid,longest_name_len,pcb->name,pcb->program_break_end - pcb->program_break,pcb->task_level == TASK_KERNEL_LEVEL ? "Kernel" : pcb->task_level == TASK_SYSTEM_SERVICE_LEVEL ? "System" : "User",pcb->task_level,pcb->cpu_clock);
        pcb = pcb->next;
    }
}

extern uint32_t phy_mem_size;

static void sys_info(){
    cpu_t *cpu = get_cpuid();

    extern void* program_break;
    extern void* program_break_end;
    uint32_t bytes = get_kernel_memory_usage() + (uint32_t)program_break_end - (uint32_t)program_break;

    extern pcb_t *running_proc_head;
    pcb_t *pcb = running_proc_head;
    while(pcb != NULL){
        pcb = pcb->next;
        if(pcb->task_level != TASK_KERNEL_LEVEL){
            bytes += (uint32_t)pcb->program_break_end - (uint32_t)pcb->program_break;
            bytes += pcb->exe_file->size;
        }
    }
    int memory = (bytes > 10485760) ? bytes/1048576 : bytes/1024;

    printk("        -*&@@@&*-        \n");
    printk("      =&@@@@@@@@@:\033[36m-----\033[39m          -----------------\n");
    printk("    .&@@@@@@@@@@:\033[36m+@@@@@:\033[39m         OSName:       CoolPotOS\n");
    printk("  .@@@@@@@@@@@:\033[36m+@@@@@@@:\033[39m         Process:      %d\n",get_all_task());
    printk("  &@@@@@@@@@@:\033[36m+@@@@@@@@:\033[39m         CPU:          %s\n",cpu->model_name);
    printk("-@@@@@@*     \033[36m&@@@@@@@=:\033[39m@-        PCI Device:   %d\n",get_pci_num());
    printk("*@@@@@&      \033[36m&@@@@@@=:\033[39m@@*        Resolution:   %d x %d\n",get_vbe_width(),get_vbe_height());
    printk("&@@@@@+      \033[36m&@@@@@=:\033[39m@@@&        Time:         %s\n",get_date_time());
    printk("@@@@@@:      \033[36m#&&&&=:\033[39m@@@@@        Console:      os_terminal\n");
    printk("&@@@@@+           +@@@@@&        Kernel:       %s\n",KERNEL_NAME);
    printk("*@@@@@@           @@@@@@*        Memory Usage: %d%s / %dMB\n",memory,bytes > 10485760 ? "MB" : "KB",(int)(phy_mem_size));
    printk("-@@@@@@*         #@@@@@@:        32-bit operating system, x86-based processor\n");
    printk(" &@@@@@@*.     .#@@@@@@& \n");
    printk(" =@@@@@@@@*---*@@@@@@@@- \n");
    printk("  .@@@@@@@@@@@@@@@@@@&.  \n");
    printk("    .#@@@@@@@@@@@@@#.    \n");
    printk("      =&@@@@@@@@@&-      \n");
    printk("        -*&@@@&+:        \n");
}

static void print_help(){
    printk("Usage <command|app_path> [argument...]\n");
    printk("help h ?                 Get shell command help.\n");
    printk("mkdir     <name>         Make a directory to vfs.\n");
    printk("mount     <dir> <dev>    Mount a file system to vfs directory.\n");
    printk("ls        [path]         List all file or directory.\n");
    printk("read      <path>         Read a text file.\n");
    printk("shutdown exit            Shutdown os.\n");
    printk("reboot                   Reboot os.\n");
    printk("sysinfo                  Get os system information.\n");
    printk("clear                    Clear terminal screen.\n");
    printk("ps                       List all processes info.\n");
}

void setup_shell(){
    printk("Welcome to CoolPotOS (%s)\n"
           " * SourceCode:     https://github.com/plos-clan/CoolPotOS\n"
           " * Website:        https://github.com/plos-clan\n"
           " System information as of %s \n"
           "  Processes:             %d\n"
           "  User login in:         Kernel\n"
           "Copyright 2024 XIAOYI12 (Build by xmake zig_cc & nasm)\n"
            ,KERNEL_NAME
            ,get_date_time(),
            get_all_task());
    char com[MAX_COMMAND_LEN];
    char *argv[MAX_ARG_NR];
    int argc = -1;
    while (1){
        printk("\033[32mKernel@localhost: \033[39m$ ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        char* com_copy[100];
        strcpy(com_copy,com);
        argc = cmd_parse(com, argv, ' ');

        if (argc == -1) {
            printk("[Shell]: Error: out of arguments buffer\n");
            continue;
        }

        if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            print_help();
        } else if(!strcmp("mkdir",argv[0]))
            mkdir(argc,argv);
        else if(!strcmp("ls",argv[0]))
            ls(argc,argv);
        else if(!strcmp("mount",argv[0]))
            mount(argc,argv);
        else if(!strcmp("read",argv[0]))
            read(argc,argv);
        else if(!strcmp("shutdown",argv[0]) || !strcmp("exit",argv[0]))
            shutdown_os();
        else if(!strcmp("reboot",argv[0]))
            reboot_os();
        else if(!strcmp("sysinfo",argv[0]))
            sys_info();
        else if(!strcmp("ps",argv[0]))
            ps();
        else if(!strcmp("clear",argv[0]))
            get_current_proc()->tty->clear(get_current_proc()->tty);
        else{
            int pid;
            if((pid = create_user_process(argv[0],com_copy,"User",TASK_APPLICATION_LEVEL)) == -1)
                printk("\033[31m[Shell]: Unknown command '%s'.\033[39m\n", argv[0]);
            pcb_t *pcb;
            do{
                pcb = found_pcb(pid);
                if(pcb == NULL) break;
            } while (1);
            printk("\n");
        }
    }
}