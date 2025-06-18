// smbios.c
// 系统管理BIOS
// 2025/3/8 By MicroFish

#include "smbios.h"
#include "kprint.h"
#include "limine.h"

LIMINE_REQUEST struct limine_smbios_request smbios_request = {.id       = LIMINE_SMBIOS_REQUEST,
                                                              .revision = 0};

LIMINE_REQUEST struct limine_efi_system_table_request efi_system_table_request = {
    .id = LIMINE_EFI_SYSTEM_TABLE_REQUEST, .revision = 0};

/* 获取SMBIOS主版本 */
int smbios_major_version(void) {
    struct EntryPoint64 *entry = (struct EntryPoint64 *)smbios_request.response->entry_64;
    if (entry == NULL) {
        struct EntryPoint32 *entry32 = (struct EntryPoint32 *)smbios_request.response->entry_32;
        return entry32 == NULL ? -1 : entry32->SMBIOSMajorVersion;
    }
    return entry->SMBIOSMajorVersion;
}

/* 获取SMBIOS次版本 */
int smbios_minor_version(void) {
    struct EntryPoint64 *entry = (struct EntryPoint64 *)smbios_request.response->entry_64;
    if (entry == NULL) {
        struct EntryPoint32 *entry32 = (struct EntryPoint32 *)smbios_request.response->entry_32;
        return entry32 == NULL ? -1 : entry32->SMBIOSMinorVersion;
    }
    return entry->SMBIOSMinorVersion;
}

bool is_uefi_bios() {
    return efi_system_table_request.response && efi_system_table_request.response->address != NULL;
}

void smbios_setup() {
    int major_version = smbios_major_version();
    int minor_version = smbios_minor_version();
    if (major_version == -1 || minor_version == -1)
        kwarn("Cannot find smbios information.");
    else
        kinfo("%s SMBIOS %d.%d.0 present.", is_uefi_bios() ? "UEFI" : "Legacy", major_version,
              minor_version);
}
