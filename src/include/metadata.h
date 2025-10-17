#pragma once

/**
 * CP_Kernel 新版内核元数据统计头文件
 * 内核编号格式:
 *    内核名称-大型更新版本.架构无关版本号.平台相关代码版本号_(git:<git哈希>)_{编译器名称 编译器版本}
 */

#define KERNEL_NAME_   "CP_Kernel"
#define KERNEL_VERSION "v0.4"

#define KERNEL_ARCH "UNKNOWN"
#define KERNEL_ARCH_VERSION "0"

#if defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH "riscv64"
#    define KERNEL_ARCH_VERSION "0"

#elif defined(__aarch64__)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH "aarch64"
#    define KERNEL_ARCH_VERSION "0"

#elif defined(__loongarch__) || defined(__loongarch64)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH "loongarch"
#    define KERNEL_ARCH_VERSION "0"

#elif defined(__x86_64__) || defined(__amd64__)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH "x86_64"
#    define KERNEL_ARCH_VERSION "39"

#elif defined(__i386__)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH "i686"
#    define KERNEL_ARCH_VERSION "44"

#endif

// 编译器判断
#if defined(__clang__)
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
    (KERNEL_NAME_ "-" KERNEL_ARCH"."KERNEL_ARCH_VERSION "-" KERNEL_VERSION "_(git:" GIT_VERSION ")_{" COMPILER_NAME       \
                  " " COMPILER_VERSION "}")
