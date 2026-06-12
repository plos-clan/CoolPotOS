#pragma once

#define KERNEL_NAME_   "CP_Kernel"
#define KERNEL_VERSION "v0.4"

#define KERNEL_ARCH         "UNKNOWN"
#define KERNEL_ARCH_VERSION "0"

#if defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH         "riscv64"
#    define KERNEL_ARCH_VERSION "1"

#elif defined(__aarch64__)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH         "aarch64"
#    define KERNEL_ARCH_VERSION "0"

#elif defined(__loongarch__) || defined(__loongarch64)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH         "loongarch64"
#    define KERNEL_ARCH_VERSION "0"

#elif defined(__x86_64__) || defined(__amd64__)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH         "x86_64"
#    define KERNEL_ARCH_VERSION "40"
#endif

// 编译器判断
#ifdef __clang__
#    define COMPILER_NAME    "clang"
#    define STRINGIFY(x)     #x
#    define EXPAND(x)        STRINGIFY(x)
#    define COMPILER_VERSION EXPAND(__clang_major__.__clang_minor__.__clang_patchlevel__)
#elif defined(__GNUC__)
#    define COMPILER_NAME    "gcc"
#    define STRINGIFY(x)     #x
#    define EXPAND(x)        STRINGIFY(x)
#    define COMPILER_VERSION EXPAND(__GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__)
#else
#    error "Unknown compiler"
#endif

// Git哈希判断
#ifndef GIT_VERSION
#    define GIT_VERSION "unknown"
#endif

#define KERNEL_NAME                                                                                \
    (KERNEL_NAME_ "-" KERNEL_ARCH "-" KERNEL_VERSION "." KERNEL_ARCH_VERSION "_(git:" GIT_VERSION  \
                  ")_{" COMPILER_NAME " " COMPILER_VERSION "}")

#define KERNEL_STACK_SIZE 0x8000               // 内核栈大小
#define KERNEL_AREA_MEM   0xf000000000000000   // 内核地址空间起始
#define SLUB_POOL_SIZE    (2 * 1024 * 1024)    // slub 分配区大小
#define SLUB_POOL_BASE    0xffff900000000000UL // slub 分配区起始地址
#define MAX_FRAMEBUFFER   10                   // 最大帧缓冲区个数识别
#define MAX_CPU_NUM       256                  // 最大支持 CPU 核心数
