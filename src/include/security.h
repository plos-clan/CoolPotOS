#pragma once

#include "types.h"

struct pthread {
    struct pthread *self;
    uintptr_t      *dtv;
    struct pthread *prev, *next; /* non-ABI */
    uintptr_t       sysinfo;
    uintptr_t       canary;
};

/**
 * 注意: 不得在调用此函数的上下文进行函数返回.
 */
__attr(always_inline) void init_stack_canary(void);

void test_stack_chk(void);
