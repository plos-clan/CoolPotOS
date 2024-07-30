#include "../include/elf.h"
#include "../include/common.h"
#include "../include/printf.h"

#define MAX(a, b) a > b ? a : b

static uint32_t div_round_up(uint32_t num, uint32_t size) {
    return (num + size - 1) / size;
}

bool elf32Validate(Elf32_Ehdr *hdr) {
    return hdr->e_ident[EI_MAG0] == ELFMAG0 && hdr->e_ident[EI_MAG1] == ELFMAG1 &&
           hdr->e_ident[EI_MAG2] == ELFMAG2 && hdr->e_ident[EI_MAG3] == ELFMAG3;
}

void load_segment(Elf32_Phdr *phdr,page_directory_t *pdt, void *elf) {
    unsigned int p = div_round_up(phdr->p_memsz, 0x1000);
    int d = phdr->p_paddr;
    if (d & 0x00000fff) {
        unsigned e = d + phdr->p_memsz;
        d = d & 0xfffff000;
        e &= 0xfffff000;
        p = (e - d) / 0x1000 + 1;
    }
    for (unsigned i = 0; i < p; i++) {
        alloc_frame(get_page(d + i * 0x1000,1,pdt,false),0,1);
    }
    memcpy(phdr->p_vaddr, elf + phdr->p_offset, phdr->p_filesz);
    if (phdr->p_memsz > phdr->p_filesz) { // 这个是bss段
        memset(phdr->p_vaddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
    }
}

uint32_t load_elf(Elf32_Ehdr *hdr,page_directory_t *pdt) {
    Elf32_Phdr *phdr = (Elf32_Phdr *)((uint32_t)hdr + hdr->e_phoff);
    for (int i = 0; i < hdr->e_phnum; i++) {
        load_segment(phdr,pdt, (void *)hdr);
        phdr++;
    }
    return hdr->e_entry;
}

void elf32LoadData(Elf32_Ehdr *elfhdr, uint8_t *ptr) {
    uint8_t *p = (uint8_t *)elfhdr;
    for (int i = 0; i < elfhdr->e_shnum; i++) {
        Elf32_Shdr *shdr =
                (Elf32_Shdr *)(p + elfhdr->e_shoff + sizeof(Elf32_Shdr) * i);

        if (shdr->sh_type != SHT_PROGBITS || !(shdr->sh_flags & SHF_ALLOC)) {
            continue;
        }

        for (int i = 0; i < shdr->sh_size; i++) {
            ptr[shdr->sh_addr + i] = p[shdr->sh_offset + i];
        }
    }
}