#pragma once

#include "cp_kernel.h"
#include "fs_subsystem.h"
#include "mem_subsystem.h"

typedef uint32_t nfds_t;

struct pollfd {
    int   fd;
    short events;
    short revents;
};
