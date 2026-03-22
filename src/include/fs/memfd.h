#pragma once

#define MFD_CLOEXEC       0x0001U
#define MFD_ALLOW_SEALING 0x0002U
#define MFD_HUGETLB       0x0004U
#define MFD_NOEXEC_SEAL   0x0008U
#define MFD_EXEC          0x0010U

#include "fs/vfs.h"
#include "lock.h"

struct memfd_ctx {
    vfs_node_t node;
    char name[64];
    uint8_t *data;
    size_t len;
    int flags;
    spin_t lock;
};

void memfd_setup();
