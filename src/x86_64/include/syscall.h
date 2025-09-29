#pragma once

#define MSR_EFER         0xC0000080 // EFER MSR寄存器
#define MSR_STAR         0xC0000081 // STAR MSR寄存器
#define MSR_LSTAR        0xC0000082 // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084

#define MAX_SYSCALLS         550
#define SYSCALL_SUCCESS      EOK
#define SYSCALL_FAULT        ((uint64_t)-(ENOSYS))
#define SYSCALL_FAULT_(name) ((uint64_t)-(name))
#define FD_SETSIZE           1024

// 一个非常取巧的宏魔法, 可以简化 syscall 函数的定义
#define __EXPAND_PARAMS(...) __VA_ARGS__
#define __CONCAT_IMPL(a, b)  a##b
#define __CONCAT(a, b)       __CONCAT_IMPL(a, b)

#define __ARGS_COUNT_IMPL(_0, _1, _2, _3, _4, _5, _6, N, ...) N

#define __ARGS_COUNT(...) __EXPAND_PARAMS(__ARGS_COUNT_IMPL(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0))

#define __SYSCALL_IMPL_0(NAME)                                                                     \
    uint64_t syscall_##NAME(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3,            \
                            uint64_t arg4, uint64_t arg5, struct syscall_regs *regs)

#define __SYSCALL_IMPL_1(NAME, P1)                                                                 \
    uint64_t syscall_##NAME(P1, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,        \
                            uint64_t arg5, struct syscall_regs *regs)

#define __SYSCALL_IMPL_2(NAME, P1, P2)                                                             \
    uint64_t syscall_##NAME(P1, P2, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5,    \
                            struct syscall_regs *regs)

#define __SYSCALL_IMPL_3(NAME, P1, P2, P3)                                                         \
    uint64_t syscall_##NAME(P1, P2, P3, uint64_t arg3, uint64_t arg4, uint64_t arg5,               \
                            struct syscall_regs *regs)

#define __SYSCALL_IMPL_4(NAME, P1, P2, P3, P4)                                                     \
    uint64_t syscall_##NAME(P1, P2, P3, P4, uint64_t arg4, uint64_t arg5, struct syscall_regs *regs)

#define __SYSCALL_IMPL_5(NAME, P1, P2, P3, P4, P5)                                                 \
    uint64_t syscall_##NAME(P1, P2, P3, P4, P5, uint64_t arg5, struct syscall_regs *regs)

#define __SYSCALL_IMPL_6(NAME, P1, P2, P3, P4, P5, P6)                                             \
    uint64_t syscall_##NAME(P1, P2, P3, P4, P5, P6, struct syscall_regs *regs)

#define __SYSCALL_DISPATCH(N, NAME, ...) __CONCAT(__SYSCALL_IMPL_, N)(NAME, ##__VA_ARGS__)

#define syscall_(NAME, ...) __SYSCALL_DISPATCH(__ARGS_COUNT(0, ##__VA_ARGS__), NAME, ##__VA_ARGS__)

#define syscall_def_(name)                                                                         \
    uint64_t syscall_##name(                                                                       \
        uint64_t arg0 __attribute__((unused)), uint64_t arg1 __attribute__((unused)),              \
        uint64_t arg2 __attribute__((unused)), uint64_t arg3 __attribute__((unused)),              \
        uint64_t arg4 __attribute__((unused)), uint64_t arg5 __attribute__((unused)),              \
        struct syscall_regs *regs __attribute__((unused)))

// arch_prctl 系统调用 code
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_SET_GS 0x1004
#define ARCH_GET_GS 0x1005

// futex 系统调用操作码
#define FUTEX_WAIT        0
#define FUTEX_WAKE        1
#define FUTEX_FD          2
#define FUTEX_REQUEUE     3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP     5
#define FUTEX_LOCK_PI     6
#define FUTEX_UNLOCK_PI   7
#define FUTEX_TRYLOCK_PI  8
#define FUTEX_WAIT_BITSET 9

// stat 文件类型标志
#define S_IFMT   00170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

// reboot 校验码
#define REBOOT_MAGIC1  0xfee1dead
#define REBOOT_MAGIC2  672274793
#define REBOOT_MAGIC2A 85072278
#define REBOOT_MAGIC2B 369367448
#define REBOOT_MAGIC2C 537993216

#define REBOOT_CMD_RESTART    0x01234567
#define REBOOT_CMD_HALT       0xCDEF0123
#define REBOOT_CMD_CAD_ON     0x89ABCDEF
#define REBOOT_CMD_CAD_OFF    0x00000000
#define REBOOT_CMD_POWER_OFF  0x4321FEDC
#define REBOOT_CMD_RESTART2   0xA1B2C3D4
#define REBOOT_CMD_SW_SUSPEND 0xD000FCE2
#define REBOOT_CMD_KEXEC      0x45584543

// Linux 兼容层系统调用编号定义
#define SYSCALL_READ        0
#define SYSCALL_WRITE       1
#define SYSCALL_OPEN        2
#define SYSCALL_CLOSE       3
#define SYSCALL_STAT        4
#define SYSCALL_FSTAT       5
#define SYSCALL_LSTAT       6
#define SYSCALL_POLL        7
#define SYSCALL_LSEEK       8
#define SYSCALL_MMAP        9
#define SYSCALL_MPROTECT    10
#define SYSCALL_MUNMAP      11
/* #define SYSCALL_BRK  12  brk 系统调用不实现*/
#define SYSCALL_SIGACTION   13
#define SYSCALL_RT_SIGMASK  14
#define SYSCALL_SIGRET      15
#define SYSCALL_IOCTL       16
#define SYSCALL_PREAD       17
#define SYSCALL_PWRITE      18
#define SYSCALL_READV       19
#define SYSCALL_WRITEV      20
#define SYSCALL_ACCESS      21
#define SYSCALL_PIPE        22
#define SYSCALL_SELECT      23
#define SYSCALL_YIELD       24
#define SYSCALL_MREMAP      25
#define SYSCALL_MINCORE     27
#define SYSCALL_DUP         32
#define SYSCALL_DUP2        33
#define SYSCALL_NANO_SLEEP  35
#define SYSCALL_GETPID      39
#define SYSCALL_SOCKET      41
#define SYSCALL_CLONE       56
#define SYSCALL_FORK        57
#define SYSCALL_VFORK       58
#define SYSCALL_EXECVE      59
#define SYSCALL_EXIT        60
#define SYSCALL_WAITPID     61
#define SYSCALL_UNAME       63
#define SYSCALL_FCNTL       72
#define SYSCALL_FTRUNCATE   77
#define SYSCALL_GETCWD      79
#define SYSCALL_CHDIR       80
#define SYSCALL_RENAME      82
#define SYSCALL_MKDIR       83
#define SYSCALL_RMDIR       84
#define SYSCALL_LINK        86
#define SYSCALL_UNLINK      87
#define SYSCALL_SYMLINK     88
#define SYSCALL_READLINK    89
#define SYSCALL_SYSINFO     99
#define SYSCALL_GETUID      102
#define SYSCALL_GETGID      104
#define SYSCALL_SETUID      105
#define SYSCALL_SETGID      106
#define SYSCALL_GETEUID     107
#define SYSCALL_GETEGID     108
#define SYSCALL_SETPGID     109
#define SYSCALL_GETPPID     110
#define SYSCALL_GETGROUPS   115
#define SYScall_GETPGID     121
#define SYSCALL_SIGSUSPEND  130
#define SYSCALL_SIGALTSTACK 131
#define SYSCALL_PRCTL       157
#define SYSCALL_ARCH_PRCTL  158
#define SYSCALL_PIVOT_ROOT  155
#define SYSCALL_G_AFFINITY  160
#define SYSCALL_CHROOT      161
#define SYSCALL_MOUNT       165
#define SYSCALL_UMOUNT2     166
#define SYSCALL_REBOOT      169
#define SYSCALL_GET_TID     186
#define SYSCALL_FUTEX       202
#define SYSCALL_GETDENTS64  217
#define SYSCALL_SETID_ADDR  218
#define SYSCALL_EXIT_GROUP  231
#define SYSCALL_C_SETTIME   227
#define SYSCALL_C_GETTIME   228
#define SYSCALL_C_GETRES    229
#define SYSCALL_C_NANOSLEEP 230
#define SYSCALL_OPENAT      257
#define SYSCALL_NEWFSTATAT  262
#define SYSCALL_UNLINKAT    263
#define SYSCALL_FACCESSAT   269
#define SYSCALL_PSELECT6    270
#define SYSCALL_PIPE2       293
#define SYSCALL_CP_F_RANGE  326
#define SYSCALL_STATX       332
#define SYSCALL_FSOPEN      430
#define SYSCALL_FACCESSAT2  439

// CoolPotOS 平台特有系统调用号定义
#define SYSCALL_MEMINFO 500
#define SYSCALL_CPUINFO 501

#include "ctype.h"
#include "krlibc.h"
#include "poll.h"
#include "signal.h"
#include "time.h"
#include "vfs.h"

struct syscall_regs {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t ds;
    uint64_t es;
    uint64_t rax;
    uint64_t func;
    uint64_t errcode;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

struct stat {
    long              st_dev;
    unsigned long     st_ino;
    unsigned long     st_nlink;
    int               st_mode;
    int               st_uid;
    int               st_gid;
    long              st_rdev;
    long long         st_size;
    long              st_blksize;
    unsigned long int st_blocks;
    struct timespec   st_atim;
    struct timespec   st_mtim;
    struct timespec   st_ctim;
    char              _pad[24];
};

typedef struct {
    vfs_node_t node;
    size_t     offset;
    size_t     fd;
    uint64_t   flags;
} fd_file_handle;

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / 8 / sizeof(long)];
} fd_set;

typedef struct {
    sigset_t *ss;
    size_t    ss_len;
} WeirdPselect6;

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct dirent {
    long           d_ino;
    long           d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[256];
};

struct statx_timestamp {
    int64_t  tv_sec;
    uint32_t tv_nsec;
    int32_t  __reserved;
};

struct statx {
    /* 0x00 */
    uint32_t stx_mask;       /* What results were written [uncond] */
    uint32_t stx_blksize;    /* Preferred general I/O size [uncond] */
    uint64_t stx_attributes; /* Flags conveying information about the file [uncond] */
    /* 0x10 */
    uint32_t stx_nlink; /* Number of hard links */
    uint32_t stx_uid;   /* User ID of owner */
    uint32_t stx_gid;   /* Group ID of owner */
    uint16_t stx_mode;  /* File mode */
    uint16_t __spare0[1];
    /* 0x20 */
    uint64_t stx_ino;             /* Inode number */
    uint64_t stx_size;            /* File size */
    uint64_t stx_blocks;          /* Number of 512-byte blocks allocated */
    uint64_t stx_attributes_mask; /* Mask to show what's supported in stx_attributes */
    /* 0x40 */
    struct statx_timestamp stx_atime; /* Last access time */
    struct statx_timestamp stx_btime; /* File creation time */
    struct statx_timestamp stx_ctime; /* Last attribute change time */
    struct statx_timestamp stx_mtime; /* Last data modification time */
    /* 0x80 */
    uint32_t               stx_rdev_major; /* Device ID of special file [if bdev/cdev] */
    uint32_t               stx_rdev_minor;
    uint32_t               stx_dev_major; /* ID of device containing file [uncond] */
    uint32_t               stx_dev_minor;
    /* 0x90 */
    uint64_t               stx_mnt_id;
    uint32_t               stx_dio_mem_align;    /* Memory buffer alignment for direct I/O */
    uint32_t               stx_dio_offset_align; /* File offset alignment for direct I/O */
    /* 0xa0 */
    uint64_t               __spare3[12]; /* Spare space for future expansion */
                                         /* 0x100 */
};

struct sysinfo {
    int64_t  uptime;    /* Seconds since boot */
    uint64_t loads[3];  /* 1, 5, and 15 minute load averages */
    uint64_t totalram;  /* Total usable main memory size */
    uint64_t freeram;   /* Available memory size */
    uint64_t sharedram; /* Amount of shared memory */
    uint64_t bufferram; /* Memory used by buffers */
    uint64_t totalswap; /* Total swap space size */
    uint64_t freeswap;  /* swap space still available */
    uint16_t procs;     /* Number of current processes */
    uint16_t pad;       /* Explicit padding for m68k */
    uint64_t totalhigh; /* Total high memory size */
    uint64_t freehigh;  /* Available high memory size */
    uint32_t mem_unit;  /* Memory unit size in bytes */
    char     _f[20 - 2 * sizeof(uint64_t) - sizeof(uint32_t)]; /* Padding: libc5 uses this.. */
};

struct cpos_meminfo {
    uint64_t used;      // 已用内存
    uint64_t available; // 未使用内存
    uint64_t commit;    // 进程已递交
    uint64_t reserved;  // 保留内存
    uint64_t all_size;  // 机器总内存
};

struct cpos_cpuinfo {
    char     vendor[16];     // CPU厂商
    char     model_name[64]; // CPU型号
    uint64_t virt_bits;      // 虚拟地址位数
    uint64_t phys_bits;      // 物理地址位数
    uint64_t cores;          // CPU核心数
    uint64_t flags;          // 特性位
};

typedef uint64_t (*syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                              struct syscall_regs *);

void setup_syscall(bool is_print);

syscall_(cp_meminfo);
syscall_(cp_cpuinfo);
