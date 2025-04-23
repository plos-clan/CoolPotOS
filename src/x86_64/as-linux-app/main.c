#include "basedefs.h"
#include "syscall.h"

#define STDIN  0
#define STDOUT 1
#define STDERR 2

static usize strlen(const char *str) {
    usize len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static int strncmp(const char *s1, const char *s2, usize n) {
    for (usize i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

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

static void say_hello() {
    int fd = open("/etc/os-release", 0);
    if (fd < 0) return;

    char  os_release[1024];
    isize bytes_read = read(fd, os_release, sizeof(os_release) - 1);
    if (bytes_read <= 0) return;
    os_release[bytes_read] = '\0';

    char *id_pos      = 0;
    char *version_pos = 0;
    char *line        = os_release;
    while (line < os_release + bytes_read) {
        if (strncmp(line, "ID=", 3) == 0) id_pos = line + 3;
        if (strncmp(line, "VERSION_CODENAME=", 17) == 0) version_pos = line + 17;
        while (*line && *line != '\n') {
            line++;
        }
        if (*line == '\n') line++;
    }

    if (id_pos && version_pos) {
        char *end = id_pos;
        while (*end && *end != '\n')
            end++;
        *end = '\0';

        end = version_pos;
        while (*end && *end != '\n')
            end++;
        *end = '\0';

        write(STDERR, "\033[32m", 5);
        wws(STDERR, "Hello there, ", 13);
        wws(STDERR, id_pos, strlen(id_pos));
        wws(STDERR, " ", 1);
        wws(STDERR, version_pos, strlen(version_pos));
        wws(STDERR, "! CoolPotOS sends its greetings from your userspace!", 52);
        write(STDERR, "\033[m\n\033[32m", 9);
        usleep(250000);
        wws(STDERR, "I might be running under you through QEMU or VirtualBox soon.", 61);
        write(STDERR, "\033[m\n\033[32m", 9);
        usleep(250000);
        wws(STDERR, "Maybe we could be friends? I promise not to take too many resources!", 68);
        write(STDERR, "\033[m\n", 4);
    }

    close(fd);
}

void _start() {
    usleep(500000);
    write(STDERR, "\033[33m", 5);
    struct utsname buf;
    if (uname(&buf) == 0) {
        wws(STDERR, "Oops! CoolPotOS kernel got lost in ", 35);
        wws(STDERR, buf.sysname, strlen(buf.sysname));
        wws(STDERR, "-land!", 6);
    } else {
        wws(STDERR, "Oops! CoolPotOS kernel is running in userspace!", 47);
    }
    write(STDERR, "\033[m\n\033[33m", 9);
    usleep(250000);
    wws(STDERR, "I'm a kernel, not a regular application!", 40);
    write(STDERR, "\033[m\n\033[33m", 9);
    usleep(250000);
    wws(STDERR, "This is like finding a fish riding a bicycle...", 47);
    write(STDERR, "\033[m\n", 4);
    usleep(500000);
    write(STDERR, "==================================================\n", 51);
    usleep(500000);
    say_hello();
    usleep(500000);
    write(STDERR, "==================================================\n", 51);
    usleep(500000);
    write(STDERR, "\033[34m", 5);
    wws(STDERR, "I really should be running on bare metal or in a virtual machine.", 65);
    write(STDERR, "\033[m\n\033[34m", 9);
    usleep(250000);
    wws(STDERR, "Please help me find my way back to where I belong.", 50);
    write(STDERR, "\033[m\n\033[34m", 9);
    usleep(250000);
    wws(STDERR, "Self-terminating now... Hope to see you in VM-land soon!", 56);
    write(STDERR, "\033[m\n", 4);
    exit(42);
    while (true) {
        asm volatile("hlt");
    }
}
