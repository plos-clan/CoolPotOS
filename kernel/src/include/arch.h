#pragma once

#include "types.h"

void arch_wait_for_interrupt();
void arch_close_interrupt();
void arch_open_interrupt();
bool arch_check_interrupt();
uintptr_t arch_get_return_address(uint32_t level);
void arch_init();
