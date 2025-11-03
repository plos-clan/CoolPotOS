#pragma once

#include "types.h"

extern uintptr_t __stack_chk_guard;

void init_stack_canary();
