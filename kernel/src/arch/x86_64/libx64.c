#include "krlibc.h"
#include "arch.h"
#include "serial.h"
#include "cpu_local.h"
#include "cpu/fsgsbase.h"

// x86 fast impl
__attribute__((naked)) static void *__memcpy_asm(void *dest, const void *src, size_t n) {
    __asm__ volatile("mov   %rdi, %rax\n\t" // 返回值 = dest
                     "mov   %eax, %r8d\n\t"
                     "neg   %r8d\n\t"
                     "and   $0x7, %r8d\n\t" // 对齐到8字节边界
                     "cmp   %r8, %rdx\n\t"
                     "cmovb %rdx, %r8\n\t" // 如果 n < 对齐字节数，就只复制 n
                     "mov   %r8, %rcx\n\t"
                     "rep movsb\n\t" // 复制对齐前的字节
                     "sub   %r8, %rdx\n\t"
                     "mov   %rdx, %rcx\n\t"
                     "shr   $0x3, %rcx\n\t"
                     "rep movsq\n\t" // 按8字节复制
                     "and   $0x7, %rdx\n\t"
                     "mov   %rdx, %rcx\n\t"
                     "rep movsb\n\t" // 复制剩余字节
                     "ret\n\t");
}

__attribute__((naked)) static void *__memset_asm(void *s, int c, size_t n) {
    __asm__ volatile("mov    %rdi, %r9\n\t"  // 保存原始 s (返回值) 到 r9，因为 rdi 会在 rep 中改变
                     "movzbq %sil, %rax\n\t" // 将 c (低8位) 零扩展到 rax
                     "movabs $0x0101010101010101, %rcx\n\t"
                     "imul   %rcx, %rax\n\t"
                     "cmp    $16, %rdx\n\t"
                     "jb     .L_tail_memset\n\t"
                     "mov    %rdi, %r8\n\t"
                     "neg    %r8\n\t"
                     "and    $0x7, %r8\n\t" // 计算距离下一个 8 字节边界差多少 (align_bytes)
                     "sub    %r8, %rdx\n\t" // n -= align_bytes (更新剩余长度)
                     "mov    %r8, %rcx\n\t"
                     "rep    stosb\n\t" // 填补头部，使 %rdi 对齐到 8 字节
                     "mov    %rdx, %rcx\n\t"
                     "shr    $0x3, %rcx\n\t"
                     "rep    stosq\n\t" // 64位高速填充
                     "and    $0x7, %rdx\n\t"
                     ".L_tail_memset:\n\t"
                     "mov    %rdx, %rcx\n\t"
                     "rep    stosb\n\t"
                     "mov    %r9, %rax\n\t"
                     "ret\n\t");
}

__attribute__((naked)) static void *__memmove_asm(void *dest, const void *src, size_t n) {
    __asm__ volatile("mov    %rdi, %rax\n\t"
                     "cmp    %rsi, %rdi\n\t"
                     "jb     .L_fwd\n\t" // dest < src: 无需反向
                     "mov    %rsi, %r8\n\t"
                     "add    %rdx, %r8\n\t"
                     "cmp    %r8, %rdi\n\t"
                     "jae    .L_fwd\n\t" // dest >= src + n: 无重叠
                     "std\n\t"           // 设置 DF=1，指针递减
                     "add    %rdx, %rdi\n\t"
                     "add    %rdx, %rsi\n\t"
                     "dec    %rdi\n\t" // 指向最后一个字节
                     "dec    %rsi\n\t"
                     "mov    %rdx, %rcx\n\t"
                     "shr    $3, %rcx\n\t"
                     "rep    movsq\n\t"
                     "mov    %rdx, %rcx\n\t"
                     "and    $7, %rcx\n\t"
                     "rep    movsb\n\t"
                     "cld\n\t" // 必须恢复 DF=0
                     "ret\n\t"
                     ".L_fwd:\n\t"
                     "mov    %rdx, %rcx\n\t"
                     "shr    $3, %rcx\n\t"
                     "rep    movsq\n\t"
                     "mov    %rdx, %rcx\n\t"
                     "and    $7, %rcx\n\t"
                     "rep    movsb\n\t"
                     "ret\n\t");
}

__attribute__((naked)) static int __memcmp_asm(const void *s1, const void *s2, size_t n) {
    __asm__ volatile("test   %rdx, %rdx\n\t"
                     "jz     .L_eq\n\t"
                     "mov    %rdx, %rcx\n\t"
                     "shr    $3, %rcx\n\t"
                     "jz     .L_tail_memcmp\n\t"
                     "repe   cmpsq\n\t"
                     "je     .L_tail_memcmp\n\t"
                     "sub    $8, %rdi\n\t"
                     "sub    $8, %rsi\n\t"
                     "mov    $8, %rcx\n\t"
                     "jmp    .L_byte_loop\n\t"
                     ".L_tail_memcmp:\n\t"
                     "mov    %rdx, %rcx\n\t"
                     "and    $7, %rcx\n\t"
                     "jz     .L_eq\n\t"
                     ".L_byte_loop:\n\t"
                     "repe   cmpsb\n\t"
                     "je     .L_eq\n\t"
                     "movzbq -1(%rdi), %rax\n\t"
                     "movzbq -1(%rsi), %rcx\n\t"
                     "sub    %ecx, %eax\n\t"
                     "ret\n\t"
                     ".L_eq:\n\t"
                     "xor    %eax, %eax\n\t"
                     "ret\n\t");
}

void *memcpy(void *dest, const void *src, const size_t n) {
    return __memcpy_asm(dest, src, n);
}

void *memset(void *s, const int c, const size_t n) {
    return __memset_asm(s, c, n);
}

void *memmove(void *dest, const void *src, const size_t n) {
    return __memmove_asm(dest, src, n);
}

int memcmp(const void *s1, const void *s2, const size_t n) {
    return __memcmp_asm(s1, s2, n);
}

void arch_wait_for_interrupt() {
    __asm__ volatile("hlt\n\t");
}

void arch_close_interrupt() {
    __asm__ volatile("cli");
}

void arch_open_interrupt() {
    __asm__ volatile("sti");
}

bool arch_check_interrupt() {
    uint64_t rflags;
    __asm__ volatile("pushfq\n\t"
                     "pop %0"
                     : "=r"(rflags));
    return (rflags & 1 << 9) != 0;
}

uintptr_t arch_get_return_address(uint32_t level) {
#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wframe-address"
#endif
    switch (level) {
    case 0:
        return (uintptr_t)__builtin_return_address(0);
    case 1:
        return (uintptr_t)__builtin_return_address(1);
    case 2:
        return (uintptr_t)__builtin_return_address(2);
    case 3:
        return (uintptr_t)__builtin_return_address(3);
    case 4:
        return (uintptr_t)__builtin_return_address(4);
    case 5:
        return (uintptr_t)__builtin_return_address(5);
    case 6:
        return (uintptr_t)__builtin_return_address(6);
    case 7:
        return (uintptr_t)__builtin_return_address(7);
    case 8:
        return (uintptr_t)__builtin_return_address(8);
    case 9:
        return (uintptr_t)__builtin_return_address(9);
    default:
        return 0;
    }
#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#endif
}

void arch_init() {
    init_serial();
    arch_bsp_cpu_setup();
    fsgsbase_init();
}
