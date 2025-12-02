#pragma once

#define FCSR_FRM_SHIFT 5
#define FCSR_FRM_MASK  (0x7U << FCSR_FRM_SHIFT)

#define FRM_RNE 0 /* Round to Nearest, ties to Even (默认) */
#define FRM_RTZ 1 /* Round Towards Zero (截断) */
#define FRM_RDN 2 /* Round Down (向负无穷) */
#define FRM_RUP 3 /* Round Up (向正无穷) */
#define FRM_RMM 4 /* Round to Nearest, ties to Max Magnitude */
#define FRM_DYN 7 /* Dynamic (由指令编码决定) */

#define FFLAGS_NX (1 << 0) /* Inexact (不精确) */
#define FFLAGS_UF (1 << 1) /* Underflow (下溢) */
#define FFLAGS_OF (1 << 2) /* Overflow (上溢) */
#define FFLAGS_DZ (1 << 3) /* Divide by Zero (除零) */
#define FFLAGS_NV (1 << 4) /* Invalid Operation (无效操作) */

#define FFLAGS_MASK 0x1F /* 所有异常标志掩码 */

/* 标准初始值：RNE舍入，无异常 */
#define FCSR_INIT_DEFAULT ((FRM_RNE << FCSR_FRM_SHIFT) | 0)

/* 其他预定义配置 */
#define FCSR_INIT_RTZ ((FRM_RTZ << FCSR_FRM_SHIFT) | 0) /* 截断模式 */
#define FCSR_INIT_RDN ((FRM_RDN << FCSR_FRM_SHIFT) | 0) /* 向下舍入 */
#define FCSR_INIT_RUP ((FRM_RUP << FCSR_FRM_SHIFT) | 0) /* 向上舍入 */

#include "ptrace.h"

typedef struct fpu_context {
    uint64_t regs[32];
    uint64_t fcsr;
} fpu_context_t;

struct arch_context_ {
    uint64_t        ra;
    uint64_t        sp;
    struct pt_regs *ctx;
    fpu_context_t   fpu_ctx;
    bool            dead;
    uint64_t        user_stack;
    uint64_t        user_stack_top;
};

#define switch_to(prev, next)                                                                      \
    do {                                                                                           \
        __asm__ volatile("addi sp, sp, -24\n\t" /* 分配栈空间 */                                   \
                         "sd s0, 0(sp)\n\t"     /* 保存帧指针 */                                   \
                         "sd t0, 8(sp)\n\t"     /* 保存临时寄存器 */                               \
                         "sd ra, 16(sp)\n\t"    /* 保存返回地址寄存器 */                           \
                         "sd sp, %0\n\t"        /* 保存当前sp */                                   \
                         "ld sp, %2\n\t"        /* 加载next的sp */                                 \
                         "la t0, 1f\n\t"        /* 获取返回地址 */                                 \
                         "sd t0, %1\n\t"        /* 保存到prev->ra */                               \
                         "ld t0, %3\n\t"        /* 加载next的ra */                                 \
                         "mv ra, t0\n\t"        /* 压入返回地址 */                                 \
                         "mv a0, %4\n\t"        /* 第一个参数 prev */                              \
                         "mv a1, %5\n\t"        /* 第二个参数 next */                              \
                         "j __switch_to\n\t"    /* 跳转到__switch_to */                            \
                         "1:\n\t"               /* 返回点 */                                       \
                         "ld ra, 16(sp)\n\t"    /* 恢复ra */                                       \
                         "ld t0, 8(sp)\n\t"     /* 恢复t0 */                                       \
                         "ld s0, 0(sp)\n\t"     /* 恢复s0 */                                       \
                         "addi sp, sp, 24\n\t"  /* 恢复栈指针 */                                   \
                         : "=m"(prev->context.sp), "=m"(prev->context.ra)                          \
                         : "m"(next->context.sp), "m"(next->context.ra), "r"(prev), "r"(next)      \
                         : "memory", "t0", "a0", "a1");                                            \
    } while (0)
