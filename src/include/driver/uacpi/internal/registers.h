#pragma once

#include <driver/uacpi/types.h>
#include <driver/uacpi/registers.h>

uacpi_status uacpi_initialize_registers(void);
void uacpi_deinitialize_registers(void);
