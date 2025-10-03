// smbios.c
// 系统管理BIOS
// 2025/3/8 By MicroFish

#include "smbios.h"
#include "hhdm.h"
#include "kprint.h"
#include "limine.h"

LIMINE_REQUEST struct limine_smbios_request smbios_request = {.id       = LIMINE_SMBIOS_REQUEST,
                                                              .revision = 0};

LIMINE_REQUEST struct limine_efi_system_table_request efi_system_table_request = {
    .id = LIMINE_EFI_SYSTEM_TABLE_REQUEST, .revision = 0};

/* 获取SMBIOS入口 */
static void *smbios_entry(void) {
    if (smbios_request.response == 0) return 0;
    if (smbios_request.response->entry_64)
        return (void *)smbios_request.response->entry_64;
    else if (smbios_request.response->entry_32)
        return (void *)smbios_request.response->entry_32;
}

/* 查找SMBIOS表 */
static const struct Header *find_smbios_type(uint8_t target_type) {
    uint8_t *table;
    uint32_t length;

    if (!smbios_request.response) return 0;

    if (smbios_request.response->entry_64) {
        struct EntryPoint64 *ep = (struct EntryPoint64 *)smbios_entry();
        table                   = (uint8_t *)phys_to_virt(ep->StructureTableAddress);
        length                  = ep->StructureTableMaxSize;
    } else {
        struct EntryPoint32 *ep = (struct EntryPoint32 *)smbios_entry();
        table                   = (uint8_t *)phys_to_virt(ep->StructureTableAddress);
        length                  = ep->StructureTableMaxSize;
    }

    uint8_t *end = table + length;
    while (table + sizeof(struct Header) <= end) {
        struct Header *hdr = (struct Header *)table;
        if (hdr->length < sizeof(struct Header)) break;
        if (hdr->type == target_type) return hdr;

        uint8_t *next = table + hdr->length;
        while (next + 1 < end && (next[0] != 0 || next[1] != 0))
            next++;
        next  += 2;
        table  = next;
    }
    return 0;
}

/* 解析SMBIOS表项字符串 */
static const char *smbios_get_string(const struct Header *hdr, int index) {
    if (!hdr || !index) return "";

    const char *str = (const char *)hdr + hdr->length;
    while (--index > 0 && *str) {
        while (*str)
            str++;
        str++;
    }

    return (*str) ? str : "";
}

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

/* Type-0 */

/* 获取BIOS厂家字符串 */
const char *smbios_bios_vendor(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 4));
}

/* 获取BIOS版本字符串 */
const char *smbios_bios_version(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 5));
}

/* 获取BIOS起始地址段 */
uint16_t smbios_bios_starting_address_segment(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return 0;
    return *(uint16_t *)((uint8_t *)hdr + 6);
}

/* 获取BIOS发布日期字符串 */
const char *smbios_bios_release_date(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 8));
}

/* 获取BIOS ROM大小 */
uint32_t smbios_bios_rom_size(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return 0;
    return ((*((uint8_t *)hdr + 9)) + 1) * 64 * 1024;
}

/* 获取BIOS特性 */
uint64_t smbios_bios_characteristics(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return 0;
    return *(uint64_t *)((uint8_t *)hdr + 10);
}

/* 获取BIOS特性扩展字节 */
uint16_t smbios_bios_characteristics_ext(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return 0;
    if (hdr->length < 0x18) return 0;
    return *(uint16_t *)((uint8_t *)hdr + 24);
}

/* 获取BIOS主要版本 */
uint8_t smbios_bios_major_release(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return 0;
    if (hdr->length < 0x19) return 0;
    return *((uint8_t *)hdr + 26);
}

/* 获取BIOS次要版本 */
uint8_t smbios_bios_minor_release(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return 0;
    if (hdr->length < 0x1a) return 0;
    return *((uint8_t *)hdr + 27);
}

/* 获取BIOS嵌入式控制器固件主要版本 */
uint8_t smbios_bios_ec_major_release(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return 0;
    if (hdr->length < 0x1b) return 0;
    return *((uint8_t *)hdr + 28);
}

/* 获取BIOS嵌入式控制器固件次要版本 */
uint8_t smbios_bios_ec_minor_release(void) {
    const struct Header *hdr = find_smbios_type(0);
    if (!hdr) return 0;
    if (hdr->length < 0x1c) return 0;
    return *((uint8_t *)hdr + 29);
}

/* Type-1 */

/* 获取系统制造商字符串 */
const char *smbios_sys_manufacturer(void) {
    const struct Header *hdr = find_smbios_type(1);
    if (!hdr) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 4));
}

/* 获取系统产品名称字符串 */
const char *smbios_sys_product_name(void) {
    const struct Header *hdr = find_smbios_type(1);
    if (!hdr) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 5));
}

/* 获取系统版本字符串 */
const char *smbios_sys_version(void) {
    const struct Header *hdr = find_smbios_type(1);
    if (!hdr) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 6));
}

/* 获取系统序列号字符串 */
const char *smbios_sys_serial_number(void) {
    const struct Header *hdr = find_smbios_type(1);
    if (!hdr) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 7));
}

/* 获取系统UUID */
void smbios_sys_uuid(uint8_t uuid[16]) {
    const struct Header *hdr = find_smbios_type(1);
    if (!hdr) {
        for (int i = 0; i < 16; i++)
            uuid[i] = 0;
        return;
    }
    const uint8_t *ptr = (const uint8_t *)hdr + 8;
    for (int i = 0; i < 16; i++)
        uuid[i] = ptr[i];
}

/* 获取系统唤醒类型 */
uint8_t smbios_sys_wakeup_type(void) {
    const struct Header *hdr = find_smbios_type(1);
    if (!hdr) return 0;
    return *((uint8_t *)hdr + 24);
}

/* 获取系统SKU编号字符串 */
const char *smbios_sys_sku_number(void) {
    const struct Header *hdr = find_smbios_type(1);
    if (!hdr) return "";
    if (hdr->length < 0x20) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 25));
}

/* 获取系统系列字符串 */
const char *smbios_sys_family(void) {
    const struct Header *hdr = find_smbios_type(1);
    if (!hdr) return "";
    if (hdr->length < 0x21) return "";
    return smbios_get_string(hdr, *((uint8_t *)hdr + 26));
}
