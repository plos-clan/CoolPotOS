#include "krlibc.h"

void arch_pause() {
    __asm__ volatile("pause");
}

void arch_wait_for_interrupt() {
    __asm__ volatile("hlt");
}

// x86 fast impl
__attribute__((naked)) void *memcpy(void *dest, const void *src, size_t n) {
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
