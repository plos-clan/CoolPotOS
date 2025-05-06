#include "acpi.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "kprint.h"
#include "scheduler.h"
#include "timer.h"

HpetInfo       *hpet_addr;
static uint32_t hpetPeriod = 0;

__attribute__((naked)) void save_registers() {
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "cli\n\t"
                     "push 0\n\t" // 对齐
                     "push 0\n\t" // 对齐
                     "push r15\n\t"
                     "push r14\n\t"
                     "push r13\n\t"
                     "push r12\n\t"
                     "push r11\n\t"
                     "push r10\n\t"
                     "push r9\n\t"
                     "push r8\n\t"
                     "push rdi\n\t"
                     "push rsi\n\t"
                     "push rbp\n\t"
                     "push rdx\n\t"
                     "push rcx\n\t"
                     "push rbx\n\t"
                     "push rax\n\t"
                     "mov rax, es\n\t"
                     "push rax\n\t"
                     "mov rax, ds\n\t"
                     "push rax\n\t"
                     "mov rdi, rsp\n\t"
                     "call timer_handle\n\t"
                     "mov rsp, rax\n\t"
                     "pop rax\n\t"
                     "mov ds, rax\n\t"
                     "pop rax\n\t"
                     "mov es, rax\n\t"
                     "pop rax\n\t"
                     "pop rbx\n\t"
                     "pop rcx\n\t"
                     "pop rdx\n\t"
                     "pop rbp\n\t"
                     "pop rsi\n\t"
                     "pop rdi\n\t"
                     "pop r8\n\t"
                     "pop r9\n\t"
                     "pop r10\n\t"
                     "pop r11\n\t"
                     "pop r12\n\t"
                     "pop r13\n\t"
                     "pop r14\n\t"
                     "pop r15\n\t"
                     "add rsp, 16\n\t" // 越过对齐
                     "sti\n\t"
                     "iretq\n\t");
}

USED registers_t *timer_handle(registers_t *reg) {
    close_interrupt;
    scheduler(reg);
    send_eoi();
    return reg;
}

void nsleep(uint64_t nano) {
    uint64_t targetTime = nanoTime();
    uint64_t after      = 0;
    loop {
        uint64_t n = nanoTime();
        if (n < targetTime) {
            after      += 0xffffffff - targetTime + n;
            targetTime  = n;
        } else {
            after      += n - targetTime;
            targetTime  = n;
        }
        if (after >= nano) { return; }
    }
}

uint64_t nanoTime() {
    if (hpet_addr == NULL) return 0;
    uint64_t mcv = hpet_addr->mainCounterValue;
    return mcv * hpetPeriod;
}

void hpet_init(Hpet *hpet) {
    hpet_addr                                     = phys_to_virt(hpet->base_address.address);
    uint32_t counterClockPeriod                   = hpet_addr->generalCapabilities >> 32;
    hpetPeriod                                    = counterClockPeriod / 1000000;
    hpet_addr->generalConfiguration              |= 1;
    *(__volatile__ uint64_t *)(hpet_addr + 0xf0)  = 0;
    register_interrupt_handler(timer, (void *)save_registers, 0, 0x8E);
    kinfo("Setup acpi hpet table (nano_time: %p).", (uint64_t)nanoTime());
}
