#include "fpu.h"
#include "task/smp.h"
#include "term/klog.h"

static void enable_sse() {
    unsigned long long cr0, cr4;
    __asm__ volatile(
        "movq %%cr0, %0\n\t"
        "andq $0xFFFB, %0\n\t"
        "orq $0x2, %0\n\t"
        "movq %0, %%cr0\n\t"
        "movq %%cr4, %1\n\t"
        "orq $0x600, %1\n\t"
        "movq %1, %%cr4\n\t"
        : "=r"(cr0), "=r"(cr4)
    );
}

void save_fpu_context(fpu_context_t *ctx) {
    __asm__ volatile("fxsave (%0)" ::"r"(ctx));
}

void restore_fpu_context(fpu_context_t *ctx) {
    __asm__ volatile("fxrstor (%0)" ::"r"(ctx));
}

void float_processor_setup() {
    enable_sse();
    cpu_local_t *current_cpu = arch_current_cpu();
    if (current_cpu == NULL) {
        kwarn("float_processor_setup: arch_current_cpu() returned NULL, skipping BSP check");
        return;
    }
    const bool is_bsp = current_cpu->id == get_bsp_cpu_id();
    if (is_bsp) {
        kinfo("Enable bsp cpu float processor.");
    }
}
