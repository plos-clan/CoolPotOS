#include "../include/shell.h"
#include "../include/queue.h"
#include "../include/graphics.h"
#include "../include/common.h"
#include "../include/task.h"
#include "../include/cmos.h"
#include "../include/vdisk.h"
#include "../include/vfs.h"
#include "../include/common.h"
#include "../include/elf.h"
#include "../include/keyboard.h"

extern Queue *key_char_queue;
extern vdisk vdisk_ctl[10];
extern bool hasFS;

static inline int isprint_syshell(int c) {
    return (c > 0x1F && c < 0x7F);
}

char getc() {
    char c;
    do{
        c = kernel_getch();
        if(c == '\b' || c == '\n') break;
    } while (!isprint_syshell(c));
    return c;
}

int gets(char *buf, int buf_size) {
    int index = 0;
    char c;
    while ((c = getc()) != '\n') {
        if (c == '\b') {
            if (index > 0) {
                index--;
                print("\b \b");
            }
        } else {
            buf[index++] = c;
            putchar(c);
        }
    }
    buf[index] = '\0';
    putchar(c);
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
        if (i == 1) print("");
        else print(" ");
        print(argv[i]);
    }
    print("\n");
}

void cmd_proc(int argc, char **argv) {
    if (argc <= 1) {
        print_proc();
        return;
    }

    if (!strcmp("list", argv[1])) {
        print_proc();
    } else if (!strcmp("kill", argv[1])) {
        if (argc <= 2) {
            printf("[Shell-PROC-kill]: If there are too few parameters.\n");
            return;
        }
        int pid = strtol(argv[2], NULL, 10);
        task_kill(pid);
    } else {
        printf("[Shell-[PROC]]: Unknown parameter\n");
        return;
    }
}

void cmd_date() {
    printf("System Time:           %s\n", get_date_time());
    printf("Memory Usage: [%dKB] | All Size: [%dMB]\n", memory_usage() / 1024,
           (KHEAP_START + KHEAP_INITIAL_SIZE) / 1024 / 1024);
    print_cpu_id();

    print("\n");
}

void cmd_ls() {
    List *ls = vfs_listfile("");
    int files = 0, dirs = 0;

    for (int i = 1; FindForCount(i, ls) != NULL; i++) {
        vfs_file *d = (vfs_file *) FindForCount(i, ls)->val;
        char *type;
        if (d->type == DIR) {
            type = "<DIR>"; //文件夹
            dirs++;
        } else if (d->type == HID) {
            type = "<HID>"; //隐藏文件
            files++;
        } else {
            type = "<FILE>"; //文件
            files++;
        }
        printf("%-13s %-6s  %d \n", d->name, type, d->size / 1024);
        kfree(d);
    }
    printf("-----------------------------------------\n");
    printf("Name          Type     Size(KB)\n"
           "All directory: %d | All Files: %d\n",dirs, files);

    DeleteList(ls);
    kfree(ls);
}

void cmd_mkdir(int argc, char **argv) {
    if (argc == 1) {
        printf("[Shell-MKDIR]: If there are too few parameters, please specify the directory name.\n");
        return;
    }

    if(!hasFS){
        printf("Cannot find fs in this disk.\n");
        return;
    }

    vfs_file *file = get_cur_file(argv[1]);

    if(file != NULL){
        printf("This file is exist.\n");
        return;
    }

    if(!vfs_createdict(argv[1])){
        printf("Illegal create.\n");
    } else printf("Create directory: %s\n", argv[1]);
}

void cmd_del(int argc, char **argv) {
    if (argc == 1) {
        print("[Shell-DEL]: If there are too few parameters, please specify the folder name.\n");
        return;
    }

    if(!hasFS){
        printf("Cannot find fs in this disk.\n");
        return;
    }

    vfs_file *file = get_cur_file(argv[1]);

    if(file == NULL){
        printf("Cannot found file.\n");
        return;
    }

    bool  b;
    if(file->type == DIR){
        b = vfs_deldir(argv[1]);
    } else b = vfs_delfile(argv[1]);
    if(!b){
        printf("Illegal delete!\n");
    }
}

void cmd_reset() {
    reset_kernel();
}

void cmd_shutdown() {
    shutdown_kernel();
}

void cmd_debug() {
    screen_clear();

    printf("%s for x86 [Version %s] \n", OS_NAME, OS_VERSION);
    printf("Copyright 2024 XIAOYI12 (Build by GCC i686-elf-tools)\n");
    extern int acpi_enable_flag;
    extern uint8_t *rsdp_address;
    printf("ACPI: Enable: %s | RSDP Address: 0x%08x\n", acpi_enable_flag ? "ENABLE" : "DISABLE",
           rsdp_address);
    int index = 0;
    print_proc_t(&index, get_current(), get_current()->next, 0);
    printf("Process Runnable: %d\n", index);
    cmd_date();
    for (int i = 0; i < 10; i++) {
        if (vdisk_ctl[i].flag) {
            vdisk vd = vdisk_ctl[i];
            char id = i + ('A');
            printf("[DISK-%c]: Size: %dMB | Name: %s\n", id, vd.size, vd.DriveName);
        }
    }
    printf("            > > > > ====[Registers Info]==== < < < <\n");
    register uint32_t eax asm("eax"),
            ecx asm("ecx"),
            esp asm("esp"),
            ebp asm("ebp"),
            ebx asm("ebx"),
            esi asm("esi"),
            edi asm("edi");
    printf("EAX: 0x%08x | EBX 0x%08x | ECX 0x%08x | ESP 0x%08x \n", eax, ebx, ecx, esp);
    printf("ESI: 0x%08x | EDI 0x%08x | EBP 0x%08x | EFLAGS 0x%08x\n", esi, edi, ebp, get_current()->context.eflags);
}

void cmd_cd(int argc, char **argv) {
    if (argc == 1) {
        print("[Shell-CD]: If there are too few parameters, please specify the path.\n");
        return;
    }
    if(!hasFS){
        printf("Cannot find fs in this disk.\n");
        return;
    }
    if (vfs_change_path(argv[1]) == 0) printf("Invalid path.\n");
}

void cmd_type(int argc,char ** argv){
    if (argc == 1) {
        print("[Shell-TYPE]: If there are too few parameters, please specify the path.\n");
        return;
    }
    if(!hasFS){
        printf("Cannot find fs in this disk.\n");
        return;
    }

    char *buffer;
    buffer = (char*) kmalloc(vfs_filesize(argv[1]));

    if(buffer == NULL){
        printf("Cannot read file.\n");
        return;
    }

    if(vfs_readfile(argv[1],buffer))
        printf("%s",buffer);
    else printf("Cannot read file.\n");

    kfree(buffer);
    print("\n");
}

void cmd_exec(int argc,char** argv){
    if (argc == 1) {
        print("[Shell-EXEC]: If there are too few parameters, please specify the path.\n");
        return;
    }
    if(vfs_filesize(argv[1]) == -1){
        print("Cannot found exec file.\n");
        return;
    }
    char buf[1024];
    sprintf(buf,"User-%s ",argv[1]);
    int32_t pid = user_process(argv[1],buf);
    klogf(pid != -1,"Launching user task PID:%d Name:%s\n",pid,buf);
}

void cmd_disk(int argc, char **argv) {
    if (argc > 1) {
        if (!strcmp("list", argv[1])) {
            printf("[Disk]: Loaded disk - ");
            for (int i = 0; i < 10; i++) {
                if (vdisk_ctl[i].flag) {
                    vdisk vd = vdisk_ctl[i];
                    char id = i + ('A');
                    printf("(%c) ", id, vd.size, vd.DriveName);
                }
            }
            printf("\n");
            return;
        } else if(!strcmp("cg", argv[1])){
            if(argc < 2){
                printf("Please type disk ID\n");
                return;
            }
            if (strlen(argv[2]) > 1) {
                printf("[DISK]: Cannot found disk.\n");
                return;
            }
            if(have_vdisk(argv[2][0])){
                vfs_change_disk(get_current(),argv[2][0]);
            } else printf("[DISK]: Cannot found disk.\n");
            return;
        }

        if (strlen(argv[1]) > 1) {
            printf("[DISK]: Cannot found disk.\n");
            return;
        }

        for (int i = 0; i < 10; i++) {
            if (vdisk_ctl[i].flag) {
                vdisk vd = vdisk_ctl[i];
                char id = i + ('A');
                if (id == (argv[1][0])) {
                    printf("[Disk(%c)]: \n"
                           "    Size: %dMB\n"
                           "    Name: %s\n"
                           "    WriteAddress: 0x%08x\n"
                           "    ReadAddress: 0x%08x\n", id, vd.size, vd.DriveName, vd.Write, vd.Read);
                    return;
                }
            }
        }
        printf("[DISK]: Cannot found disk.\n");
        return;
    }
    printf("[Disk]: Loaded disk - ");
    for (int i = 0; i < 10; i++) {
        if (vdisk_ctl[i].flag) {
            vdisk vd = vdisk_ctl[i];
            char id = i + ('A');
            printf("(%c) ", id, vd.size, vd.DriveName);
        }
    }
    printf("\n");
}

char *user() {
    printf("\n\nPlease type your username and password.\n");
    char com[MAX_COMMAND_LEN];
    char *username = "admin";
    char *password = "12345678";
    while (1) {
        print("username/> ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        if (!strcmp(username, com)) {
            break;
        } else printf("Cannot found username, Please check your info.\n");
    }
    while (1) {
        print("password/> ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        if (!strcmp(password, com)) {
            break;
        } else printf("Unknown password, Please check your info.\n");
    }
    return username;
}

void setup_shell() {
    asm("sti");
    printf("Welcome to %s %s (CPOS Kernel i386)\n"
           "\n"
           " * SourceCode:     https://github.com/xiaoyi1212/CoolPotOS\n"
           " * Website:        https://github.com/plos-clan\n"
           "\n"
           " System information as of %s \n"
           "\n"
           "  Processes:             %d\n"
           "  Users logged in:       default\n"
           "  Memory usage:          %d B        \n"
           "\n"
           "Copyright 2024 XIAOYI12 (Build by GCC i686-elf-tools)\n"
           ,OS_NAME
           ,OS_VERSION
           ,get_date_time()
           ,get_procs()
           ,memory_usage());
    char com[MAX_COMMAND_LEN];
    char *argv[MAX_ARG_NR];
    int argc = -1;
    char *buffer[255];

    extern char root_disk;
    vfs_change_disk(get_current(),root_disk);

    while (1) {
        if(hasFS) vfs_getPath(buffer);
        else{
            buffer[0] = 'n';
            buffer[1] = 'o';
            buffer[2] = '_';
            buffer[3] = 'f';
            buffer[4] = 's';
            buffer[5] = '\0';
        }
        printf("\03343cd80;default@localhost: \0334169E1;%s\\\033c6c6c6;$ ", buffer);
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;

        argc = cmd_parse(com, argv, ' ');

        if (argc == -1) {
            print("[Shell]: Error: out of arguments buffer\n");
            continue;
        }

        if (!strcmp("version", argv[0]))
            printf("%s for x86 [%s]\n", OS_NAME, OS_VERSION);
        else if (!strcmp("type", argv[0]))
            cmd_type(argc, argv);
        else if (!strcmp("clear", argv[0]))
            screen_clear();
        else if (!strcmp("proc", argv[0]))
            cmd_proc(argc, argv);
        else if (!strcmp("sysinfo", argv[0]))
            cmd_date();
        else if (!strcmp("ls", argv[0]))
            cmd_ls();
        else if (!strcmp("mkdir", argv[0]))
            cmd_mkdir(argc, argv);
        else if (!strcmp("del", argv[0]) || !strcmp("rm", argv[0]))
            cmd_del(argc, argv);
        else if (!strcmp("reset", argv[0]))
            cmd_reset();
        else if (!strcmp("shutdown", argv[0]) || !strcmp("exit", argv[0]))
            cmd_shutdown();
        else if (!strcmp("debug", argv[0]))
            cmd_debug();
        else if (!strcmp("disk", argv[0]))
            cmd_disk(argc, argv);
        else if (!strcmp("cd", argv[0]))
            cmd_cd(argc, argv);
        else if (!strcmp("exec",argv[0]))
            cmd_exec(argc,argv);
        else if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            print("-=[CoolPotShell Helper]=-\n");
            print("help ? h              Print shell help info.\n");
            print("version               Print os version.\n");
            print("type       <name>     Read a file.\n");
            print("ls                    List all files.\n");
            print("mkdir      <name>     Make a directory.\n");
            print("del rm     <name>     Delete a file.\n");
            print("sysinfo               Print system info.\n");
            print("proc [kill<pid>|list] Lists all running processes.\n");
            print("reset                 Reset OS.\n");
            print("shutdown exit         Shutdown OS.\n");
            print("debug                 Print os debug info.\n");
            print("disk[list|<ID>|cg<ID>]List or view disks.\n");
            print("cd  <path>            Change shell top directory.\n");
            print("exec <path>           Execute a application.\n");
        } else printf("\033ff3030;[Shell]: Unknown command '%s'.\033c6c6c6;\n", argv[0]);
    }
}
