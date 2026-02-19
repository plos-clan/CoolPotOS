#pragma once

#include "ptrace.h"

// 一个非常取巧的宏魔法, 可以简化 syscall 函数的定义
#define __EXPAND_PARAMS(...) __VA_ARGS__
#define __CONCAT_IMPL(a, b)  a##b
#define __CONCAT(a, b)       __CONCAT_IMPL(a, b)

#define __ARGS_COUNT_IMPL(_0, _1, _2, _3, _4, _5, _6, N, ...) N

#define __ARGS_COUNT(...) __EXPAND_PARAMS(__ARGS_COUNT_IMPL(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0))

#define __SYSCALL_IMPL_0(NAME)                                                                     \
    uint64_t syscall_##NAME(                                                                       \
        uint64_t arg0,                                                                             \
        uint64_t arg1,                                                                             \
        uint64_t arg2,                                                                             \
        uint64_t arg3,                                                                             \
        uint64_t arg4,                                                                             \
        uint64_t arg5,                                                                             \
        struct syscall_regs *regs                                                                  \
    )

#define __SYSCALL_IMPL_1(NAME, P1)                                                                 \
    uint64_t syscall_##NAME(                                                                       \
        P1,                                                                                        \
        uint64_t arg1,                                                                             \
        uint64_t arg2,                                                                             \
        uint64_t arg3,                                                                             \
        uint64_t arg4,                                                                             \
        uint64_t arg5,                                                                             \
        struct syscall_regs *regs                                                                  \
    )

#define __SYSCALL_IMPL_2(NAME, P1, P2)                                                             \
    uint64_t syscall_##NAME(                                                                       \
        P1,                                                                                        \
        P2,                                                                                        \
        uint64_t arg2,                                                                             \
        uint64_t arg3,                                                                             \
        uint64_t arg4,                                                                             \
        uint64_t arg5,                                                                             \
        struct syscall_regs *regs                                                                  \
    )

#define __SYSCALL_IMPL_3(NAME, P1, P2, P3)                                                         \
    uint64_t syscall_##NAME(                                                                       \
        P1, P2, P3, uint64_t arg3, uint64_t arg4, uint64_t arg5, struct syscall_regs *regs         \
    )

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
        uint64_t arg0 __attribute__((unused)),                                                     \
        uint64_t arg1 __attribute__((unused)),                                                     \
        uint64_t arg2 __attribute__((unused)),                                                     \
        uint64_t arg3 __attribute__((unused)),                                                     \
        uint64_t arg4 __attribute__((unused)),                                                     \
        uint64_t arg5 __attribute__((unused)),                                                     \
        struct syscall_regs *regs __attribute__((unused))                                          \
    )

#define SYSCALL_FAULT_(name) ((uint64_t)-(name))

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

// mremap 标志
#define MREMAP_MAYMOVE   1
#define MREMAP_FIXED     2
#define MREMAP_DONTUNMAP 4

// fnctl
#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_SETOWN        8
#define F_GETOWN        9
#define F_SETSIG        10
#define F_GETSIG        11
#define F_DUPFD_CLOEXEC 1030

#define MS_RDONLY      1  /* 只读挂载 */
#define MS_NOSUID      2  /* 忽略 SUID/SGID */
#define MS_NODEV       4  /* 禁止访问设备文件 */
#define MS_NOEXEC      8  /* 禁止执行 */
#define MS_SYNCHRONOUS 16 /* 同步写入 */
#define MS_REMOUNT     32 /* 重新挂载已挂载点 */
#define MS_MANDLOCK    64
#define MS_DIRSYNC     128
#define MS_NOATIME     1024
#define MS_NODIRATIME  2048
#define MS_BIND        4096  /* 绑定挂载 */
#define MS_MOVE        8192  /* 挂载点移动 */
#define MS_REC         16384 /* 递归 */
#define MS_PRIVATE     (1 << 18)
#define MS_SHARED      (1 << 20)
#define MS_SLAVE       (1 << 19)
#define MS_UNBINDABLE  (1 << 17)

// futex 系统调用操作码
#define FUTEX_WAIT           0
#define FUTEX_WAKE           1
#define FUTEX_FD             2
#define FUTEX_REQUEUE        3
#define FUTEX_CMP_REQUEUE    4
#define FUTEX_WAKE_OP        5
#define FUTEX_LOCK_PI        6
#define FUTEX_UNLOCK_PI      7
#define FUTEX_TRYLOCK_PI     8
#define FUTEX_WAIT_BITSET    9
#define FUTEX_WAKE_BITSET    10
#define FUTEX_PRIVATE_FLAG   128
#define FUTEX_CLOCK_REALTIME 256
#define FUTEX_CMD_MASK       0x7f

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

#define FD_SETSIZE 1024

#define WNOHANG    1
#define WUNTRACED  2
#define WCONTINUED 8

#define SEEK_SET  0 /* Seek from beginning of file.  */
#define SEEK_CUR  1 /* Seek from current position.  */
#define SEEK_END  2 /* Seek from end of file.  */
#define SEEK_DATA 3
#define SEEK_HOLE 4

#define RLIMIT_CPU        0
#define RLIMIT_FSIZE      1
#define RLIMIT_DATA       2
#define RLIMIT_STACK      3
#define RLIMIT_CORE       4
#define RLIMIT_RSS        5
#define RLIMIT_NPROC      6
#define RLIMIT_NOFILE     7
#define RLIMIT_MEMLOCK    8
#define RLIMIT_AS         9
#define RLIMIT_LOCKS      10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE   12
#define RLIMIT_NICE       13
#define RLIMIT_RTPRIO     14
#define RLIMIT_RTTIME     15
#define RLIMIT_NLIMITS    16

#define LINUX_REBOOT_MAGIC1  0xfee1dead
#define LINUX_REBOOT_MAGIC2  672274793
#define LINUX_REBOOT_MAGIC2A 85072278
#define LINUX_REBOOT_MAGIC2B 369367448
#define LINUX_REBOOT_MAGIC2C 537993216

#define LINUX_REBOOT_CMD_RESTART    0x01234567
#define LINUX_REBOOT_CMD_HALT       0xCDEF0123
#define LINUX_REBOOT_CMD_CAD_ON     0x89ABCDEF
#define LINUX_REBOOT_CMD_CAD_OFF    0x00000000
#define LINUX_REBOOT_CMD_POWER_OFF  0x4321FEDC
#define LINUX_REBOOT_CMD_RESTART2   0xA1B2C3D4
#define LINUX_REBOOT_CMD_SW_SUSPEND 0xD000FCE2
#define LINUX_REBOOT_CMD_KEXEC      0x45584543

#include "fs/vfs.h"
#include "task/poll.h"
#include "task/signal.h"
#include "timer.h"
#include "types.h"

struct iovec {
    void *iov_base;
    size_t iov_len;
};

#include "fs/sockfs.h"

struct stat {
    long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    int st_mode;
    int st_uid;
    int st_gid;
    long st_rdev;
    long long st_size;
    long st_blksize;
    unsigned long int st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    char _pad[24];
};

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / 8 / sizeof(long)];
} fd_set;

typedef struct {
    sigset_t *ss;
    size_t ss_len;
} WeirdPselect6;

struct dirent {
    long d_ino;
    long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

struct statx_timestamp {
    int64_t tv_sec;
    uint32_t tv_nsec;
    int32_t __reserved;
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
    uint32_t stx_rdev_major; /* Device ID of special file [if bdev/cdev] */
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major; /* ID of device containing file [uncond] */
    uint32_t stx_dev_minor;
    /* 0x90 */
    uint64_t stx_mnt_id;
    uint32_t stx_dio_mem_align;    /* Memory buffer alignment for direct I/O */
    uint32_t stx_dio_offset_align; /* File offset alignment for direct I/O */
    /* 0xa0 */
    uint64_t __spare3[12]; /* Spare space for future expansion */
                           /* 0x100 */
};

struct sysinfo {
    int64_t uptime;                                        /* Seconds since boot */
    uint64_t loads[3];                                     /* 1, 5, and 15 minute load averages */
    uint64_t totalram;                                     /* Total usable main memory size */
    uint64_t freeram;                                      /* Available memory size */
    uint64_t sharedram;                                    /* Amount of shared memory */
    uint64_t bufferram;                                    /* Memory used by buffers */
    uint64_t totalswap;                                    /* Total swap space size */
    uint64_t freeswap;                                     /* swap space still available */
    uint16_t procs;                                        /* Number of current processes */
    uint16_t pad;                                          /* Explicit padding for m68k */
    uint64_t totalhigh;                                    /* Total high memory size */
    uint64_t freehigh;                                     /* Available high memory size */
    uint32_t mem_unit;                                     /* Memory unit size in bytes */
    char _f[20 - 2 * sizeof(uint64_t) - sizeof(uint32_t)]; /* Padding: libc5 uses this.. */
};

typedef struct {
    int val[2];
} __kernel_fsid_t;

struct statfs {
    uint64_t f_type;
    uint64_t f_bsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    __kernel_fsid_t f_fsid;
    uint64_t f_namelen;
    uint64_t f_frsize;
    uint64_t f_flags;
    uint64_t f_spare[4];
};

struct rlimit {
    size_t rlim_cur;
    size_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss;
    long ru_ixrss;
    long ru_idrss;
    long ru_isrss;
    long ru_minflt;
    long ru_majflt;
    long ru_nswap;
    long ru_inblock;
    long ru_oublock;
    long ru_msgsnd;
    long ru_msgrcv;
    long ru_nsignals;
    long ru_nvcsw;
    long ru_nivcsw;
};

struct tms {
    long tms_utime;
    long tms_stime;
    long tms_cutime;
    long tms_cstime;
};

// Linux capability 版本号
#define _LINUX_CAPABILITY_VERSION_1 0x19980330
#define _LINUX_CAPABILITY_VERSION_2 0x20071026
#define _LINUX_CAPABILITY_VERSION_3 0x20080522

typedef struct {
    uint32_t version;
    int pid;
} cap_user_header_t;

typedef struct {
    uint32_t effective;
    uint32_t permitted;
    uint32_t inheritable;
} cap_user_data_t;

void arch_enable_syscall();

// fs syscall
syscall_(open, char *path0, uint64_t flags, uint64_t mode);
syscall_(close, int fd);
syscall_(write, int fd, uint8_t *buffer, size_t size);
syscall_(read, int fd, uint8_t *buffer, size_t size);
syscall_(writev, int fd, struct iovec *iov, int iovcnt);
syscall_(readv, int fd, struct iovec *iov, int iovcnt0);
syscall_(stat, char *fn, struct stat *buf);
syscall_(lstat, char *fn, struct stat *buf);
syscall_(ioctl, int fd, size_t options, void *arg2);
syscall_(dup2, int fd, int newfd);
syscall_(dup3, int oldfd, int newfd, int flags);
syscall_(dup, int fd);
syscall_(getcwd, char *buffer, size_t length);
syscall_(chdir, char *s);
syscall_(fcntl, int fd, int cmd, uint64_t arg);
syscall_(mount, char *dev_name, char *dir_name, char *type, uint64_t flags, void *data);
syscall_(fstat, int fd, struct stat *buf);
syscall_(poll, struct pollfd *fds_user, size_t nfds, size_t timeout);
syscall_(umount2, char *path0);
syscall_(lseek, int fd, size_t offset, size_t whence);
syscall_(pread, int fd, uint8_t *buffer);
syscall_(pwrite, int fd, uint8_t *buffer);
syscall_(
    copy_file_range,
    int fd_in,
    uint64_t *off_in,
    int fd_out,
    uint64_t *off_out,
    size_t len,
    uint64_t flags
);
syscall_(ftruncate);
syscall_(rename, char *oldpath, char *newpath);
syscall_(symlink, char *name, char *new);
syscall_(link, char *name, char *new);
syscall_(select, int nfds, uint8_t *read, uint8_t *write, uint8_t *except, struct timeval *timeout);
syscall_(
    pselect6,
    uint64_t nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    struct timespec *timeout,
    WeirdPselect6 *weirdPselect6
);
syscall_(getdents, int fd, struct dirent *dents, size_t size);
syscall_(newfstatat, int dirfd, char *pathname, struct stat *buf, uint64_t flags);
syscall_(statx, int dirfd, char *pathname, uint64_t flags, uint64_t mask, struct statx *buff);
syscall_(pipe2, int *pipefd, uint64_t flags);
syscall_(pipe, int *pipefd);
syscall_(unlink, char *name);
syscall_(rmdir, char *name);
syscall_(unlinkat, int dirfd, char *name);
syscall_(access, char *filename);
syscall_(mkdir, char *name, uint64_t mode);
syscall_(mkdirat, int dirfd, char *name, uint64_t mode);
syscall_(mknod, char *path, uint32_t mode, uint32_t dev);
syscall_(readlink, char *path, char *buf, uint64_t size);
syscall_(chmod, char *path, uint64_t mode);
syscall_(fchmod, int fd, uint64_t mode);
syscall_(fchmodat, int dirfd, char *path, uint64_t mode, int flags);
syscall_(sendfile, int out_fd, int in_fd, uint64_t *offset_ptr, size_t count);
syscall_(openat, int dirfd, char *name, uint64_t flags, uint64_t mode);
syscall_(faccessat, int dirfd, char *pathname, uint64_t mode);
syscall_(faccessat2, int dirfd, char *pathname, uint64_t mode, uint64_t flag);
syscall_(statfs, char *path, struct statfs *buf);
syscall_(chroot, char *path);
syscall_(chown, const char *filename, uint64_t uid, uint64_t gid);
syscall_(fchown, int fd, uint64_t uid, uint64_t gid);
syscall_(lchown, const char *filename, uint64_t uid, uint64_t gid);
syscall_(fchownat, int dirfd, const char *path, uint64_t uid, uint64_t gid, int flags);
syscall_(utimensat, int dfd, const char *pathname, struct timespec *ntimes, int flags);
syscall_(futimensat, int dfd, const char *pathname, struct timeval *utimes);
syscall_(umask, uint64_t mask);
syscall_(sync);

// socket syscall
syscall_(socket, int domain, int type, int protocol);
syscall_(socketpair, int domain, int type, int protocol, int *sv);
syscall_(bind, int sockfd, struct sockaddr *addr, uint64_t addrlen);
syscall_(listen, int sockfd, int backlog);
syscall_(accept, int sockfd, struct sockaddr *addr, uint64_t *addrlen);
syscall_(connect, int sockfd, struct sockaddr *addr, uint64_t addrlen);
syscall_(
    sendto,
    int sockfd,
    const void *buf,
    size_t len,
    int flags,
    struct sockaddr *dest_addr,
    uint64_t addrlen
);
syscall_(
    recvfrom,
    int sockfd,
    void *buf,
    size_t len,
    int flags,
    struct sockaddr *src_addr,
    uint64_t *addrlen
);
syscall_(sendmsg, int sockfd, struct msghdr *msg, int flags);
syscall_(recvmsg, int sockfd, struct msghdr *msg, int flags);
syscall_(shutdown, int sockfd, int how);
syscall_(setsockopt, int sockfd, int level, int optname, const void *optval, uint64_t optlen);
syscall_(getsockopt, int sockfd, int level, int optname, void *optval, uint64_t *optlen);
syscall_(getsockname, int sockfd, struct sockaddr *addr, uint64_t *addrlen);
syscall_(getpeername, int sockfd, struct sockaddr *addr, uint64_t *addrlen);

// epoll syscall
syscall_(epoll_create1, int flags);
syscall_(epoll_ctl, int epfd, int op, int fd, const struct epoll_event *event);
syscall_(epoll_wait, int epfd, struct epoll_event *events, int maxevents, int timeout);
syscall_(
    epoll_pwait,
    int epfd,
    struct epoll_event *events,
    int maxevents,
    int timeout,
    const sigset_t *sigmask,
    size_t sigsetsize
);

// eventfd syscall
syscall_(eventfd2, uint64_t initval, int flags);

// proc syscall
syscall_(exit, int exit_code);
syscall_(set_tid_address, const int *tidptr);
syscall_(getpid);
syscall_(exit_group, int exit_code);
syscall_(getuid);
syscall_(getgid);
syscall_(yield);
syscall_(setpgid, pid_t pid, pid_t pgid);
syscall_(getpgid, pid_t pid);
syscall_(getsid, pid_t pid);
syscall_(setsid);
syscall_(getppid);
syscall_(ssetmask, int how, const sigset_t *nset, sigset_t *oset);
syscall_(sigaltstack, altstack_t *old_stack, const altstack_t *new_stack);
syscall_(sig_action, int sig, const sigaction_t *action, sigaction_t *oldaction);
syscall_(sigpending, sigset_t *set, size_t sigsetsize);
syscall_(
    sigtimedwait,
    const sigset_t *set,
    siginfo_t *info,
    const struct timespec *timeout,
    size_t sigsetsize
);
syscall_(sigqueueinfo, pid_t pid, int sig, const siginfo_t *info);
syscall_(sigsuspend, const sigset_t *mask, size_t sigsetsize);
syscall_(signal, int sig, void *handler);
syscall_(sigret);
syscall_(getegid);
syscall_(geteuid);
syscall_(waitpid, pid_t pid, int *status, uint64_t options, struct rusage *rusage);
syscall_(futex, int *uaddr, int op, int val, const struct timespec *time, int timeout);
syscall_(get_tid);
syscall_(fork);
syscall_(vfork);
syscall_(execve, char *path, char **argv, char **envp);
syscall_(prctl, int option);
syscall_(clone, uint64_t flags, uint64_t stack, int *parent_tid, int *child_tid, uint64_t tls);
syscall_(get_rlimit, uint64_t resource, struct rlimit *lim);
syscall_(
    prlimit64, uint64_t pid, int resource, const struct rlimit *new_rlim, struct rlimit *old_rlim
);
syscall_(getresgid, int *rgid, int *egid, int *sgid);
syscall_(getresuid, int *ruid, int *euid, int *suid);
syscall_(kill, int pid, int sig);
syscall_(capget, cap_user_header_t *header, cap_user_data_t *data);
syscall_(capset, cap_user_header_t *header, const cap_user_data_t *data);

// mem syscall
syscall_(
    mmap, uint64_t addr, size_t length, uint64_t prot, uint64_t flags, int fd, uint64_t offset
);
syscall_(munmap, uint64_t addr, size_t size);
syscall_(
    mremap,
    uint64_t old_addr,
    uint64_t old_size,
    uint64_t new_size,
    uint64_t flags,
    uint64_t new_addr
);
syscall_(madvise, uint64_t addr, size_t length, int advice);
syscall_(mprotect, uint64_t addr, size_t length, uint64_t prot);
syscall_(mincore, uint64_t addr, uint64_t size, uint64_t vec);

// os syscall
syscall_(uname, struct utsname *utsname);
syscall_(clock_gettime, uint64_t arg0, struct timespec *ts);
syscall_(clock_getres,uint64_t arg0, struct timespec *res);
syscall_(getgroups, int count, int *gid_list);
syscall_(nano_sleep, const void *time_handle);
syscall_(sysinfo, struct sysinfo *info);
syscall_(sys_log, int type, const char *buf, size_t len);
syscall_(setitimer, int which, const struct itimerval *value, struct itimerval *old);
syscall_(reboot, int magic1, int magic2, uint32_t cmd, void *arg);
syscall_(getrandom, void *buffer, size_t len, uint32_t flags);
syscall_(times, struct tms *buf);
syscall_(getrusage, int who, struct rusage *usage);
