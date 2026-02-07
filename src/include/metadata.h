#pragma once

/**
 * CP_Kernel 新版内核元数据统计头文件
 * 内核编号格式:
 *    内核名称-大型更新版本.架构无关版本号.平台相关代码版本号_(git:<git哈希>)_{编译器名称 编译器版本}
 *
 * ARCH_HAS_OPTIMIZED_MEMCPY 该架构有 memcpy 高速指令集优化, 取消 krlibc 原版实现
 * ARCH_HAS_OPTIMIZED_MEMSET 该架构有 memset 高速指令集优化
 *
 */

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
#    define KERNEL_ARCH                "x86_64"
#    define KERNEL_ARCH_VERSION        "40"
#    define ARCH_HAS_OPTIMIZED_MEMCPY  1
#    define ARCH_HAS_OPTIMIZED_MEMSET  1
#    define ARCH_HAS_OPTIMIZED_MEMCMP  1
#    define ARCH_HAS_OPTIMIZED_MEMMOVE 1
#elif defined(__i386__)
#    undef KERNEL_ARCH
#    undef KERNEL_ARCH_VERSION
#    define KERNEL_ARCH         "i686"
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
    (KERNEL_NAME_ "-" KERNEL_ARCH "-" KERNEL_VERSION "." KERNEL_ARCH_VERSION "_(git:" GIT_VERSION  \
                  ")_{" COMPILER_NAME " " COMPILER_VERSION "}")

// 内核属性
#define MAX_CPU               256                // 最大支持CPU核心数 256
#define KERNEL_HEAP_START     0xffff900000000000 // 内核堆起始地址
#define KERNEL_HEAP_SIZE      0x1600000          // 内核堆初始大小 25MB (可扩容)
//#define STACK_SIZE            0x4000               //32768                // 栈大小
#define STACK_SIZE            0x8000               //32768                // 栈大小
#define BIG_USER_STACK        999424               // 用户栈大小，要对齐到页
#define EHDR_START_ADDR       0x0000300000000000   // ELF头起始地址
#define INTERPRETER_EHDR_ADDR 0x0000200000000000   // 链接器ELF头起始地址
#define INTERPRETER_BASE_ADDR 0x0000100000000000   // 链接器基址起始地址
#define USER_MMAP_START       0x0000400000000000UL // 用户堆映射起始地址
#define KERNEL_AREA_MEM       0xf000000000000000   // 内核地址空间起始
#define DRIVER_AREA_MEM       0xffffb00000000000   // 驱动恒等映射空间偏移
#define KASAN_SHADOW_BASE     0xffffd00000000000   // KASAN 影子内存映射基址
#define KASAN_MONITOR_START   0xffffffff80000000UL // KASAN 监控起始
#define KASAN_MONITOR_END     0xffffffffc0000000UL // KASON 监控终止
#define SCHED_TIMER_SPEED     100                  // 调度时钟频率 100Hz
#define MAX_STACK_SIZE        131072ULL            // 增强栈大小 128k
#define MAX_FRAMEBUFFER       10                   // 最大帧缓冲区个数识别
#define MAX_LOAD_MODULE       256                  // 最大模块加载数
#define SENDFILE_BUF_SIZE     1024                 // sendfile 系统调用缓冲区
#define VT_TTY_MAX            63                   // tty会话个数
#define CMD_BUF_SIZE          2048                 // 命令行最大长度缓冲区
#define MAX_ARGC              256                  // 命令行最大个数
#define MAX_TASK_FD           0x10000 // 任务最大文件描述符个数 (为 sys_rlimit 提供, 实际内核不限制大小)

// 内核编译配置选项
#ifndef EEVDF_SCHEDULER
#    define EEVDF_SCHEDULER 1 // 是否启用EEVDF调度器
#endif

#ifndef HEAP_CHECK
#    define HEAP_CHECK 0 // 启用内核堆双端越界检查
#endif

#ifndef MODULE_CHECK
#    define MODULE_CHECK 1 // 启动内核模块签名校验
#endif

#ifndef KASAN_CHECK
#    define KASAN_CHECK 0 // 启动 KASAN 内核内存检查
#endif
