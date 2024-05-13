#include "../include/shell.h"
#include "../include/queue.h"
#include "../include/graphics.h"
#include "../include/common.h"
#include "../include/task.h"
#include "../include/cmos.h"
#include "../include/vdisk.h"

extern Queue *key_char_queue;
extern vdisk vdisk_ctl[10];

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
    printf("Memory Usage: [%dKB] | All Size: [%dMB]\n",memory_usage()/1024,(KHEAP_START+KHEAP_INITIAL_SIZE)/1024/1024);
    print_cpu_id();
    vga_writestring("\n");
}

void cmd_ls() {

}

void cmd_mkdir(int argc, char **argv) {
    if (argc == 1) {
        printf("\033[Shell-MKDIR]: If there are too few parameters, please specify the directory name.\036\n");
        return;
    }
    printf("Create directory: %s\n",argv[1]);

}

void cmd_del(int argc, char **argv) {
    if (argc == 1) {
        vga_writestring("\033[Shell-DEL]: If there are too few parameters, please specify the folder name.\036\n");
        return;
    }

}

void cmd_reset(){
    reset_kernel();
}

void cmd_shutdown(){
    shutdown_kernel();
}

void cmd_debug(){
    vga_clear();
    printf("%s for x86 [Version %s] \n",OS_NAME, OS_VERSION);
    printf("\032Copyright 2024 XIAOYI12 (Build by GCC i686-elf-tools)\036\n");
    extern int acpi_enable_flag;
    extern uint8_t *rsdp_address;
    printf("ACPI: Enable: %s | RSDP Address: 0x%08x\n",acpi_enable_flag?"\035ENABLE\036":"\033DISABLE\036",rsdp_address);
    int index = 0;
    print_proc_t(&index,get_current(),get_current()->next,0);
    printf("Process Runnable: %d\n",index);
    cmd_date();
    for (int i = 0; i < 10; i++) {
        if (vdisk_ctl[i].flag) {
            vdisk vd = vdisk_ctl[i];
            char id = i + ('A');
            printf("[DISK-%c]: Size: %dMB | Name: %s\n",id,vd.size,vd.DriveName);
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
    printf("EAX: 0x%08x | EBX 0x%08x | ECX 0x%08x | ESP 0x%08x \n",eax,ebx,ecx,esp);
    printf("ESI: 0x%08x | EDI 0x%08x | EBP 0x%08x | EFLAGS 0x%08x\n",esi,edi,ebp,get_current()->context.eflags);
}

void cmd_disk(int argc,char **argv){
    if(argc > 1){
        if(!strcmp("list",argv[1])){
            printf("[Disk]: Loaded disk - ");
            for (int i = 0; i < 10; i++) {
                if (vdisk_ctl[i].flag) {
                    vdisk vd = vdisk_ctl[i];
                    char id = i + ('A');
                    printf("(%c) ",id,vd.size,vd.DriveName);
                }
            }
            printf("\n");
            return;
        }

        if(strlen(argv[1]) > 1){
            printf("\033[DISK]: Cannot found disk.\036\n");
            return;
        }

        for (int i = 0; i < 10; i++) {
            if (vdisk_ctl[i].flag) {
                vdisk vd = vdisk_ctl[i];
                char id = i + ('A');
                if(id == (argv[1][0])){
                    printf("[Disk(%c)]: \n"
                           "    Size: %dMB\n"
                           "    Name: %s\n"
                           "    WriteAddress: 0x%08x\n"
                           "    ReadAddress: 0x%08x\n",id,vd.size,vd.DriveName,vd.Write,vd.Read);
                    return;
                }
            }
        }
        printf("\033[DISK]: Cannot found disk.\036\n");
        return;
    }
    printf("[Disk]: Loaded disk - ");
    for (int i = 0; i < 10; i++) {
        if (vdisk_ctl[i].flag) {
            vdisk vd = vdisk_ctl[i];
            char id = i + ('A');
            printf("(%c) ",id,vd.size,vd.DriveName);
        }
    }
    printf("\n");
}

char* user(){
    printf("\n\nPlease type your username and password.\n");
    char com[MAX_COMMAND_LEN];
    char* username = "admin";
    char* password = "12345678";
    while (1){
        print("username/> ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        if(!strcmp(username,com)){
            break;
        } else printf("Cannot found username, Please check your info.\n");
    }
    while (1){
        print("password/> ");
        if (gets(com, MAX_COMMAND_LEN) <= 0) continue;
        if(!strcmp(password,com)){
            break;
        } else printf("Unknown password, Please check your info.\n");
    }
    return username;
}

void setup_shell(){
    char* user1 = user();
    vga_clear();
    printf("%s for x86 [Version %s] \n",OS_NAME, OS_VERSION);
    printf("\032Copyright 2024 XIAOYI12 (Build by GCC i686-elf-tools)\036\n");

    char com[MAX_COMMAND_LEN];
    char *argv[MAX_ARG_NR];
    int argc = -1;

    while (1) {
        printf("\035%s/>\036 ",user1);
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
        else if (!strcmp("mkdir", argv[0]))
            cmd_mkdir(argc, argv);
        else if (!strcmp("del", argv[0]) || !strcmp("rm", argv[0]))
            cmd_del(argc, argv);
        else if (!strcmp("reset", argv[0]))
            cmd_reset();
        else if (!strcmp("shutdown", argv[0])||!strcmp("exit", argv[0]))
            cmd_shutdown();
        else if (!strcmp("debug", argv[0]))
            cmd_debug();
        else if (!strcmp("disk", argv[0]))
            cmd_disk(argc,argv);
        else if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            vga_writestring("-=[\037CoolPotShell Helper\036]=-\n");
            vga_writestring("help ? h              \032Print shell help info.\036\n");
            vga_writestring("version               \032Print os version.\036\n");
            vga_writestring("echo       <msg>      \032Print message.\036\n");
            vga_writestring("ls                    \032List all files.\036\n");
            vga_writestring("mkdir      <name>     \032Make a directory.\036\n");
            vga_writestring("del rm     <name>     \032Delete a file.\036\n");
            vga_writestring("sysinfo               \032Print system info.\036\n");
            vga_writestring("proc [kill<pid>|list] \032Lists all running processes.\036\n");
            vga_writestring("reset                 \032Reset OS.\036\n");
            vga_writestring("shutdown exit         \032Shutdown OS.\036\n");
            vga_writestring("debug                 \032Print os debug info.\036\n");
            vga_writestring("disk  [list|<ID>]     \032List or view disks.\036\n");
        } else printf("\033[Shell]: Unknown command '%s'.\036\n", argv[0]);
    }
}
