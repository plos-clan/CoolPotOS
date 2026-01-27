#pragma once

#include "neotype.h"

typedef enum logger_level {
    INFO,
    WARN,
    FAILED,
} logger_level;

void neo_acpi_logger_warn(const char *str, ...);
void neo_acpi_logger_info(const char *str, ...);
void neo_acpi_logger_failed(const char *str, ...);

#define log_info(...) neo_acpi_logger_info(__VA_ARGS__)

#define log_warn(...) neo_acpi_logger_warn(__VA_ARGS__)

#define log_error(...) neo_acpi_logger_failed(__VA_ARGS__)
