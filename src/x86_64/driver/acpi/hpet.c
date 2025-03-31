#include "acpi.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "kprint.h"
#include "krlibc.h"
#include "pcb.h"
#include "scheduler.h"
#include "timer.h"

HpetInfo       *hpet_addr;
static uint32_t hpetPeriod = 0;

extern void save_registers(); // switch.S

/*ASM CALL*/ registers_t *timer_handle(registers_t *reg) {
    close_interrupt;
    scheduler(reg);
    send_eoi();
    return reg;
}

void usleep(uint64_t nano) {
    uint64_t targetTime = nanoTime();
    uint64_t after      = 0;
    infinite_loop {
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
