#include "../include/elf.h"
#include "../include/common.h"
#include "../include/printf.h"

#define max(a, b) a > b ? a : b

static uint32_t div_round_up(uint32_t num, uint32_t size) {
    return (num + size - 1) / size;
}

bool elf32Validate(Elf32_Ehdr *hdr) {
    return hdr->e_ident[EI_MAG0] == ELFMAG0 && hdr->e_ident[EI_MAG1] == ELFMAG1 &&
           hdr->e_ident[EI_MAG2] == ELFMAG2 && hdr->e_ident[EI_MAG3] == ELFMAG3;
}

uint32_t elf32_get_max_vaddr(Elf32_Ehdr *hdr) {
    Elf32_Phdr *phdr = (Elf32_Phdr *)((uint32_t)hdr + hdr->e_phoff);
    uint32_t        max  = 0;
    for (int i = 0; i < hdr->e_phnum; i++) {
        // 如果memsz大于filesz 说明这是bss段，我们以最大的为准
        uint32_t size = max(phdr->p_filesz, phdr->p_memsz);
        max      = max(max, phdr->p_vaddr + size);
        phdr++;
    }
    return max;
}

void load_segment(Elf32_Phdr *phdr,page_directory_t *dir, void *elf) {
    size_t hi = PADDING_UP(phdr->p_paddr + phdr->p_memsz, 0x1000);
    size_t lo = PADDING_DOWN(phdr->p_paddr, 0x1000);
    for (size_t i = lo; i < hi; i += 0x1000) {
        alloc_frame(get_page(i,1,dir,false),0,1);
    }
    printf("VDDR: %08x elf: %08x offset: %08x filesz: %08x elf+offset: %08x\n",phdr->p_vaddr, elf , phdr->p_offset, phdr->p_filesz, elf + phdr->p_offset);
    memcpy((void *)phdr->p_vaddr, elf + phdr->p_offset, phdr->p_filesz);
    if (phdr->p_memsz > phdr->p_filesz) { // 这个是bss段
        memset((void *)(phdr->p_vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
    }
}

uint32_t load_elf(Elf32_Ehdr *hdr,page_directory_t *dir) {
    Elf32_Phdr *phdr = (Elf32_Phdr *)((uint32_t)hdr + hdr->e_phoff);
    for (int i = 0; i < hdr->e_phnum; i++) {
        load_segment(phdr, (void *)hdr,dir);
        phdr++;
    }
    return hdr->e_entry;
}


void elf32LoadData(Elf32_Ehdr *elfhdr, uint8_t *ptr) {
    uint8_t *p = (uint8_t *) elfhdr;
    for (int i = 0; i < elfhdr->e_shnum; i++) {
        Elf32_Shdr *shdr =
                (Elf32_Shdr *) (p + elfhdr->e_shoff + sizeof(Elf32_Shdr) * i);

        if (shdr->sh_type != SHT_PROGBITS || !(shdr->sh_flags & SHF_ALLOC)) {
            continue;
        }

        for (int i = 0; i < shdr->sh_size; i++) {
            ptr[shdr->sh_addr + i] = p[shdr->sh_offset + i];
        }
    }
}