#include "acpi.h"
#include "krlibc.h"
#include "kprint.h"
#include "limine.h"
#include "hhdm.h"


XSDT *xsdt;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
        .id = LIMINE_RSDP_REQUEST,
        .revision = 0
};

void *find_table(const char *name) {
    uint64_t entry_count = (xsdt->h.Length - 32) / 8;
    uint64_t *t = (uint64_t *) ((char *) xsdt + offsetof(XSDT, PointerToOtherSDT));
    for (uint64_t i = 0; i < entry_count; i++) {
        uint64_t ptr = (uint64_t) phys_to_virt((uint64_t) *(t + i));
        uint8_t signa[5] = {0};
        memcpy(signa, ((struct ACPISDTHeader *) ptr)->Signature, 4);
        if (memcmp(signa, name, 4) == 0) {
            return (void *) ptr;
        }
    }
    return NULL;
}

void acpi_setup() {
    struct limine_rsdp_response *response = rsdp_request.response;

    RSDP *rsdp = (RSDP *) response->address;
    if (rsdp == NULL) {
        kwarn("Cannot find acpi rsdp table.");
        return;
    }

    xsdt = (XSDT *) rsdp->xsdt_address;
    if (xsdt == NULL) {
        kwarn("Cannot find acpi xsdt table.");
        return;
    }
    xsdt = (XSDT *) phys_to_virt((uint64_t) xsdt);

    void *hpet = find_table("HPET");
    if (hpet == NULL) {
        kwarn("Cannot find acpi hpet table.");
        return;
    } else
        hpet_init(hpet);

    void *apic = find_table("APIC");
    if (apic == NULL) {
        kwarn("Cannot find acpi apic table.");
        return;
    } else
        apic_setup(apic);


    void* facp = find_table("FACP");
    if(facp == NULL) {
        kwarn("Cannot find acpi facp table.");
        return;
    } else
        setup_facp(facp);
}
