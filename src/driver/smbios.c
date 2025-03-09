// smbios.c
// 系统管理BIOS
// 2025/3/8 By MicroFish

#include "smbios.h"
#include "limine.h"
#include "kprint.h"

__attribute__((used, section(".limine_requests")))
static __volatile__ struct limine_smbios_request smbios_request = {
        .id = LIMINE_SMBIOS_REQUEST
};

/* 获取SMBIOS主版本 */
int smbios_major_version(void) {
    struct EntryPoint64 *entry = (struct EntryPoint64 *) smbios_request.response->entry_64;
    if(entry == NULL){
        return -1;
    }
    return entry->SMBIOSMajorVersion;
}

/* 获取SMBIOS次版本 */
int smbios_minor_version(void) {
    struct EntryPoint64 *entry = (struct EntryPoint64 *) smbios_request.response->entry_64;
    if(entry == NULL){
        return -1;
    }
    return entry->SMBIOSMinorVersion;
}

void smbios_setup(){
    int major_version = smbios_major_version();
    int minor_version = smbios_minor_version();
    if(major_version == -1 || minor_version == -1)
        kwarn("Cannot find smbios information.");
    else kinfo("SMBIOS %d.%d.0 present.",major_version, minor_version);
}
