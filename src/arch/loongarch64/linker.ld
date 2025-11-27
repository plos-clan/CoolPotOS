OUTPUT_FORMAT(elf64-loongarch)
ENTRY(_start)

SECTIONS {
    KERNEL_VMA = 0xffff800000000000;
    /* 内核物理加载地址 */
    . = 0x90000000;

    kernel_phys_start = .;
    kernel_virt_start = . + KERNEL_VMA;

    /* 低地址部分 - 用于启动 */
    .bootstrap : {
        *(.boot)
        *(.boot.*)
    }

    /* 内核虚拟地址 - 高半部分 */
    . += KERNEL_VMA;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    text_start_va = .;
    .text (text_start_va) : AT(text_start_va - KERNEL_VMA) {
        *(.text)
        *(.text.*)
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    rodata_start_va = .;
    .rodata (rodata_start_va) : AT(rodata_start_va - KERNEL_VMA) {
        *(.rodata)
        *(.rodata.*)
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    ksymtab_start_va = .;
    .ksymtab (ksymtab_start_va) : AT(ksymtab_start_va - KERNEL_VMA) {
        __ksymtab_start = .;
        KEEP(*(.ksymtab))
        __ksymtab_end = .;
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    data_start_va = .;
    .data (data_start_va) : AT(data_start_va - KERNEL_VMA) {
        *(.sdata)
        *(.sdata.*)
        *(.data)
        *(.data.*)
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    _bss_start = .;

    bss_start_va = .;
    .bss (bss_start_va) : AT(bss_start_va - KERNEL_VMA) {
        *(.sbss)
        *(.sbss.*)
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    _bss_end = .;

    kernel_virt_end = .;
    kernel_phys_end = . - KERNEL_VMA;

    /* Discard .note.* and .eh_frame* since they may cause issues on some hosts. */
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}