#include "lib/neoacpi/neo_logger.h"
#include "lib/neoacpi/neo_impl.h"
#include "lib/neoacpi/neo_stdlib.h"
#include "lib/neoacpi/neoacpi.h"

void neo_acpi_logger_raw(const char *str, ...) {
    char buf[CONFIG_ACPI_PLAIN_LOG_BUFFER_SIZE];

    int ret;

    neo_acpi_va_list vlist;
    neo_acpi_va_start(vlist, str);

    ret = neo_acpi_vsnprintf(buf, sizeof(buf), str, vlist);
    if (ret < 0) return;

    neo_acpi_info_logger(buf);

    neo_acpi_va_end(vlist);
}
