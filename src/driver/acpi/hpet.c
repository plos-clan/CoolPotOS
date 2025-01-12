#include "timer.h"
#include "acpi.h"
#include "kprint.h"
#include "hhdm.h"

HpetInfo *hpet_addr;
static uint32_t hpetPeriod = 0;

void hpet_init(Hpet *hpet) {
    hpet_addr = phys_to_virt(hpet->base_address.address);
    uint32_t counterClockPeriod = hpet_addr->generalCapabilities >> 32;
    hpetPeriod = counterClockPeriod / 1000000;
    hpet_addr->generalConfiguration |= 1;
    kinfo("HPET address: 0x%p", hpet_addr);
}
