#include "krlibc.h"
#include "io.h"
#include "timer.h"
#include "exec/elf.h"

void arch_pause() {
    __asm__ volatile("nop");
}

void arch_wait_for_interrupt() {
    __asm__ volatile("idle 0" ::: "memory");
}

void arch_open_interrupt() {
    csr_write(LOONGARCH_CSR_CRMD, 1ULL << 0);
}

void arch_close_interrupt() {
    clear_csr(LOONGARCH_CSR_CRMD, 1ULL << 0);
}

bool arch_check_interrupt(void) {
    uint64_t crmd = csr_read(LOONGARCH_CSR_CRMD);
    return (crmd & (1ULL << 0)) != 0; /* check IE bit */
}

bool arch_elf_test_head(Elf64_Ehdr *ehdr) {
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1
        || ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3
        || ehdr->e_version != EV_CURRENT || ehdr->e_ehsize != sizeof(Elf64_Ehdr)
        || ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        return false;
    }

    if (
        ehdr->e_ident[4] != 2 // 64-bit
                              // TODO loongarch64
    ) {
        return false;
    }

    return true;
}

void arch_pci_legacy_enum() {
}

uint64_t sched_clock() {
    return nano_time();
}

uint64_t nano_time() {
    return 0;
}

bool arch_get_random_bytes(uint8_t *buf, size_t size) {
    return false;
}
