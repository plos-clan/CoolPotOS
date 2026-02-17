#pragma once

#define SSTATUS_SIE (1UL << 1)

#define csr_read(csr)                                                                              \
    ({                                                                                             \
        uint64_t __v;                                                                              \
        __asm__ volatile("csrr %0, " #csr : "=r"(__v) : : "memory");                               \
        __v;                                                                                       \
    })

#define csr_write(csr, val)                                                                        \
    ({                                                                                             \
        uint64_t __v = (uint64_t)(val);                                                            \
        __asm__ volatile("csrw " #csr ", %0" : : "r"(__v) : "memory");                             \
    })

#define csr_set(csr, val)                                                                          \
    ({                                                                                             \
        uint64_t __v = (uint64_t)(val);                                                            \
        __asm__ volatile("csrs " #csr ", %0" : : "r"(__v) : "memory");                             \
    })

#define csr_clear(csr, val)                                                                        \
    ({                                                                                             \
        uint64_t __v = (uint64_t)(val);                                                            \
        __asm__ volatile("csrc " #csr ", %0" : : "r"(__v) : "memory");                             \
    })

#define SSTATUS_GET_FS(sstatus)     (((sstatus) >> 13) & 0b11)
#define SSTATUS_SET_FS(sstatus, fs) ((sstatus) |= (((uint64_t)(fs) & 0b11) << 13))
