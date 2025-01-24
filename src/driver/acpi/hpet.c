#include "timer.h"
#include "acpi.h"
#include "kprint.h"
#include "hhdm.h"
#include "krlibc.h"
#include "isr.h"
#include "scheduler.h"

HpetInfo *hpet_addr;
static uint32_t hpetPeriod = 0;

__IRQHANDLER void timer_handle(interrupt_frame_t *frame) {
    send_eoi();
}

void usleep(uint64_t nano) {
    uint64_t targetTime = nanoTime();
    uint64_t after = 0;
    while (1) {
        uint64_t n = nanoTime();
        if (n < targetTime) {
            after += 0xffffffff - targetTime + n;
            targetTime = n;
        } else {
            after += n - targetTime;
            targetTime = n;
        }
        if (after >= nano) {
            return;
        }
    }
}

uint64_t nanoTime() {
    if (hpet_addr == NULL) return 0;
    uint64_t mcv = hpet_addr->mainCounterValue;
    return mcv * hpetPeriod;
}

void hpet_init(Hpet *hpet) {
    hpet_addr = phys_to_virt(hpet->base_address.address);
    uint32_t counterClockPeriod = hpet_addr->generalCapabilities >> 32;
    hpetPeriod = counterClockPeriod / 1000000;
    hpet_addr->generalConfiguration |= 1;
    register_interrupt_handler(timer, (void *) timer_handle, 0, 0x8E);
    kinfo("Setup acpi hpet table (nano_time: %d).", (uint64_t) nanoTime());
}
