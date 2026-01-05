#include "exec/elf.h"
#include "io.h"
#include "krlibc.h"

void arch_pause() {
    __asm__ volatile("nop");
}

void arch_wait_for_interrupt() {
    __asm__ volatile("wfi");
}

void arch_open_interrupt() {
    csr_set(sstatus, (1 << 1)); /* SIE */
}

void arch_close_interrupt() {
    csr_clear(sstatus, (1 << 1)); /* SIE */
}

bool arch_check_interrupt(void) {
    uint64_t sstatus;
    __asm__ volatile("csrr %0, sstatus" : "=r"(sstatus));
    return (sstatus & SSTATUS_SIE) != 0;
}

bool arch_elf_test_head(Elf64_Ehdr *ehdr) {
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
        ehdr->e_version != EV_CURRENT || ehdr->e_ehsize != sizeof(Elf64_Ehdr) ||
        ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        return false;
    }

    if (ehdr->e_ident[4] != 2 || // 64-bit
        ehdr->e_machine != 0xF3  // riscv64
    ) {
        return false;
    }

    return true;
}

void arch_pci_legacy_enum() {}

void arch_cpu_init(){
    // SUM
    csr_set(sstatus, (1UL << 18));
    // FPU
    csr_set(sstatus, (3UL << 13));
}
