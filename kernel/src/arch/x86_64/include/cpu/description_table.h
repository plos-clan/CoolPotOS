#pragma once

#define SA_RPL3 3

#define SA_RPL_MASK      0xFFFC
#define SA_TI_MASK       0xFFFB
#define GET_SEL(cs, rpl) ((cs & SA_RPL_MASK & SA_TI_MASK) | (rpl))

#include "types.h"

struct gdt_register {
    uint16_t size;
    void *ptr;
} __attribute__((packed));

struct tss {
    uint32_t unused0;
    uint64_t rsp[3];
    uint64_t unused1;
    uint64_t ist[7];
    uint64_t unused2;
    uint16_t unused3;
    uint16_t iopb;
} __attribute__((packed));

typedef struct tss tss_t;
typedef uint64_t gdt_entries_t[7];
