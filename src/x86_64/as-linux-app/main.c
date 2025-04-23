#include "linuxapp/basedefs.h"
#include "linuxapp/syscall.h"
#include "sprintf.h"
#include "krlibc.h"

#define STDIN  0
#define STDOUT 1
#define STDERR 2

static int open(const char *filename, int flags) {
    return __syscall(2, filename, flags);
}

static int close(int fd) {
    return __syscall(3, fd);
}

struct timespec {
    i64 tv_sec;
    i64 tv_nsec;
};

static int usleep(usize usec) {
    struct timespec ts = {0, usec * 1000};
    return __syscall(35, &ts, 0);
}

static isize write(int fd, const char *buf, usize count) {
    return __syscall(1, fd, buf, count);
}

static isize wws(int fd, const char *buf, usize count) {
    for (usize i = 0; i < count; i++) {
        __syscall(1, fd, &buf[i], 1);
        if (buf[i] == ',' || buf[i] == '.' || buf[i] == '!' || buf[i] == '?') {
            usleep(200000);
        } else {
            usleep(50000);
        }
    }
    return count;
}

static isize read(int fd, char *buf, usize count) {
    return __syscall(0, fd, buf, count);
}

static void exit(int status) {
    __syscall(60, status);
    __builtin_unreachable();
}

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

static int uname(struct utsname *buf) {
    return __syscall(63, buf);
}

void printf(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    stbsp_vsprintf(buf, fmt, args);
    va_end(args);
    write(STDOUT, buf, strlen(buf));
}

#define MAX_ARGS 16

void parse_args(char *input, char **argv) {
    int argc = 0;
    while (*input && argc < MAX_ARGS - 1) {
        // 跳过前导空格
        while (*input == ' ') input++;
        if (*input == '\0') break;

        argv[argc++] = input;

        // 找下一个空格并替换成 \0
        while (*input && *input != ' ') input++;
        if (*input == ' ') *input++ = '\0';
    }
    argv[argc] = NULL;
}

static void help(){
    printf("Usage <command|app_path> [argument...]\n");
    printf("help h ?                 Get shell help info.\n");
    printf("exit                     Exit coolpotos.\n");
    printf("clear                    Clear screen.\n");
}

void _start() {
    usleep(500000);
    printf("\033[33mWelcome to Linux subsystem for CoolPotOS! (%s)\n",KERNEL_NAME);

    struct utsname info;
    __syscall(63, &info);
    printf("\033[32m%s(%s) %s\n",info.sysname,info.machine,info.release);

    char buffer[256];
    char cwd[256];

    while (1) {
        long ret = __syscall(SYS_getcwd, (long)cwd, (long)sizeof(cwd));
        printf("\033[32m%s(%s)\033[0m@%s \033[34m%s\033[0m$ ",info.sysname,info.machine,info.nodename,ret >= 0 ? cwd : "?");
        int len = read(STDIN, buffer, 256);
        buffer[len - 1] = 0;
        if(strlen(buffer) <= 0) {
            continue;
        }

        char *argv[MAX_ARGS];
        parse_args(buffer, argv);

        if (strcmp(argv[0], "exit") == 0) {
            exit(0);
        } else if(strcmp(argv[0], "help") == 0||strcmp(argv[0], "?") == 0||strcmp(argv[0], "h") == 0){
            help();
            continue;
        } else if(strcmp(argv[0], "clear") == 0) {
            printf("\033[H\033[J");
            continue;
        }

        char work_path[256];
        if(argv[0][0] == '/') {
            stbsp_snprintf(work_path, sizeof(work_path), "%s", argv[0]);
        } else if(argv[0][0] == '.') {
            stbsp_snprintf(work_path, sizeof(work_path), "%s/%s", cwd, argv[0]);
        } else {
            stbsp_snprintf(work_path, sizeof(work_path), "/usr/bin/%s", argv[0]);
        }
        argv[0] = work_path;

        char *envp[] = { "PATH=/bin:/usr/bin", NULL };

        pid_t pid = (pid_t)(__syscall(SYS_fork,0));
        if (pid == 0) {
            __syscall(SYS_execve,argv[0], argv, envp);
            printf("unknown command '%s'\n", buffer);
            exit(1);
        } else {
            __syscall(SYS_wait4,pid, NULL, 0);
        }
    }

    while (true) {
        asm volatile("hlt");
    }
}
