#pragma once

#include "neotype.h"

void neo_acpi_logger_raw(const char *str, ...);

#define log_info(...)                                                                              \
    do {                                                                                           \
        neo_acpi_logger_raw("[  INFO  ]: ");                                                       \
        neo_acpi_logger_raw(__VA_ARGS__);                                                          \
        neo_acpi_logger_raw("\n");                                                                 \
    } while (0)

#define log_warn(...)                                                                              \
    do {                                                                                           \
        neo_acpi_logger_raw("[  WARN  ]: ");                                                       \
        neo_acpi_logger_raw(__VA_ARGS__);                                                          \
        neo_acpi_logger_raw("\n");                                                                 \
    } while (0)

#define log_error(...)                                                                             \
    do {                                                                                           \
        neo_acpi_logger_raw("[ FAILED ]: ");                                                       \
        neo_acpi_logger_raw(__VA_ARGS__);                                                          \
        neo_acpi_logger_raw("\n");                                                                 \
    } while (0)
