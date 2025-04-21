#include "acpi.h"
#include "hhdm.h"
#include "kprint.h"
#include "krlibc.h"
#include "limine.h"

#define load_table(name, func)                                                                     \
    do {                                                                                           \
        void *name = find_table(#name);                                                            \
        if (name == NULL) {                                                                        \
            kwarn("Cannot find acpi " #name " table.");                                            \
            return;                                                                                \
        } else                                                                                     \
            func(name);                                                                            \
    } while (0)

XSDT *xsdt;

LIMINE_REQUEST struct limine_rsdp_request rsdp_request = {.id = LIMINE_RSDP_REQUEST, .revision = 0};

void *find_table(const char *name) {
    uint64_t  entry_count = (xsdt->h.Length - 32) / 8;
    uint64_t *t           = (uint64_t *)((char *)xsdt + offsetof(XSDT, PointerToOtherSDT));
    for (uint64_t i = 0; i < entry_count; i++) {
        uint64_t ptr      = (uint64_t)phys_to_virt((uint64_t)*(t + i));
        uint8_t  signa[5] = {0};
        memcpy(signa, ((struct ACPISDTHeader *)ptr)->Signature, 4);
        if (memcmp(signa, name, 4) == 0) { return (void *)ptr; }
    }
    return NULL;
}

void acpi_setup() {
    struct limine_rsdp_response *response = rsdp_request.response;

    RSDP *rsdp = (RSDP *)response->address;
    if (rsdp == NULL) {
        kwarn("Cannot find acpi rsdp table.");
        return;
    }

    xsdt = (XSDT *)rsdp->xsdt_address;
    if (xsdt == NULL) {
        kwarn("Cannot find acpi xsdt table.");
        return;
    }
    xsdt = (XSDT *)phys_to_virt((uint64_t)xsdt);

    load_table(HPET, hpet_init);
    load_table(APIC, apic_setup);
    load_table(MCFG, pci_setup);
    load_table(FACP, setup_facp);

    //   load_table(BGRT, bgrt_setup); // TODO 先空着，等以后补上
}
