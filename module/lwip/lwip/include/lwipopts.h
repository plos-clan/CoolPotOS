#ifndef CUSTOM_LWIPOPTS_H
#define CUSTOM_LWIPOPTS_H

#include "arch/cc.h"
#include "lock.h"

// extending on lwip/include/lwip/opt.h

#define LWIP_PROVIDE_ERRNO   1
#define MEM_LIBC_MALLOC      1
#define LWIP_NO_CTYPE_H      1
#define LWIP_NO_UNISTD_H     1
#define LWIP_NO_LIMITS_H     1
#define LWIP_TIMEVAL_PRIVATE 0
#define CUSTOM_IOVEC         1

#define TCPIP_MBOX_SIZE 32

#define LWIP_RAW   1
#define LWIP_DHCP  1
#define LWIP_DNS   1
#define LWIP_DEBUG 1

// raise connection limits
#define MEMP_NUM_NETCONN 100
#define MEMP_NUM_TCP_PCB 100

// raise the server buffer (forced to do second)
#define TCP_SND_BUF      8192
#define MEMP_NUM_TCP_SEG (2 * TCP_SND_QUEUELEN)

// optimizations
#define TCP_WND               (16 * TCP_MSS)
#define LWIP_CHKSUM_ALGORITHM 3

#define SYS_LIGHTWEIGHT_PROT 0
#define LWIP_COMPAT_SOCKETS  0

typedef struct mboxBlock {
    struct mboxBlock *next;

    task_t *task;
    bool    write;
} mboxBlock;

typedef struct {
    spin_t lock;

    mboxBlock *firstBlock;

    bool   invalid;
    int    ptrRead;
    int    ptrWrite;
    int    size;
    void **msges;
} sys_mbox_t;

typedef uint64_t sys_thread_t;
typedef struct {
    spin_t   lock;
    uint32_t cnt;
    bool     invalid;
} sys_sem_t;
typedef spin_t sys_mutex_t;
// typedef lwip_mbox sys_mbox_t;

// #define SYS_LIGHTWEIGHT_PROT 1
typedef uint8_t sys_prot_t;

#define LWIP_PLATFORM_ASSERT(x)                                                                    \
    do {                                                                                           \
        serial_fprintk("Assertion \"%s\" failed at line %d in %s\n", x, __LINE__, __FILE__);       \
        while (1) {                                                                                \
            arch_yield();                                                                          \
        }                                                                                          \
    } while (0)

#define LWIP_PLATFORM_DIAG(x)                                                                      \
    do {                                                                                           \
        serial_fprintk x;                                                                          \
    } while (0)

extern uint64_t next;

static inline int rand(void) {
    next = next * 1103515245 + 12345;
    return (uint32_t)(next / 65536) % 32768;
}

#define LWIP_RAND() ((u32_t)rand())

#endif
