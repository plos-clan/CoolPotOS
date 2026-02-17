#pragma once

// CSR 寄存器定义
#define LOONGARCH_CSR_CRMD 0x0
#define LOONGARCH_CSR_PRMD 0x1
#define LOONGARCH_CSR_EUEN 0x2
#define LOONGARCH_CSR_ECFG 0x4
#define LOONGARCH_CSR_ESTAT 0x5
#define LOONGARCH_CSR_ERA 0x6
#define LOONGARCH_CSR_BADV 0x7
#define LOONGARCH_CSR_EENTRY 0xc
#define LOONGARCH_CSR_TLBRENTRY 0x88
#define LOONGARCH_CSR_KS0 0x30
#define LOONGARCH_CSR_KS1 0x31

// CRMD 位定义
#define CSR_CRMD_IE 0x4 // 全局中断使能

#define csr_read(reg)                                                                              \
    ({                                                                                             \
        uint64_t __val;                                                                            \
        __asm__ volatile("csrrd %0, %1\n\t" : "=r"(__val) : "i"(reg));                             \
        __val;                                                                                     \
    })

#define csr_write(reg, val)                                                                        \
    do {                                                                                           \
        __asm__ volatile("csrwr %0, %1\n\t" : : "r"(val), "i"(reg));                               \
    } while (0)

#define clear_csr(reg, mask)                                                                       \
    do {                                                                                           \
        uint64_t val = csr_read(reg);                                                              \
        csr_write(reg, val & ~mask);                                                               \
    } while (0)

#define csr_xchg(reg, val, mask)                                                                   \
    ({                                                                                             \
        uint64_t __old;                                                                            \
        asm volatile("csrxchg %0, %1, %2\n\t" : "=r"(__old) : "r"(val), "i"(reg));                 \
        __old;                                                                                     \
    })
