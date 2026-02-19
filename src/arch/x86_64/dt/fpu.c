#include "fpu.h"
#include "task/smp.h"
#include "term/klog.h"

static void enable_sse() {
    __asm__ volatile("movq %cr0, %rax\n\t"
                     "and  $0xFFFB, %ax\n\t"
                     "or   $0x2,%ax\n\t"
                     "movq %rax,%cr0\n\t"
                     "movq %cr4,%rax\n\t"
                     "or   $(3<<9),%ax\n\t"
                     "movq %rax,%cr4\n\t");
}

void save_fpu_context(fpu_context_t *ctx) {
    __asm__ volatile("fxsave (%0)" ::"r"(ctx));
    // __asm__ volatile("fnsave %0" : "=m"(ctx->fxsave_area)::"memory");
}

void restore_fpu_context(fpu_context_t *ctx) {
    __asm__ volatile("fxrstor (%0)" ::"r"(ctx));
    // __asm__ volatile("frstor %0" : : "m"(ctx->fxsave_area) : "memory");
}

void float_processor_setup() {
    enable_sse();
    const bool is_bsp = arch_current_cpu()->id == get_bsp_cpu_id();
    if (is_bsp) {
        kinfo("Enable bsp cpu float processor.");
    }
}
