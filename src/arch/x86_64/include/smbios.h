// smbios.h
// 系统管理BIOS头文件
// 2025/3/8 By MicroFish
// GPL-3.0 License

#pragma once
#include "ctype.h"

/* 入口点结构 */
struct EntryPoint64 {
    char     AnchorString[5];       // 锚字符串，64位为"_SM3_"
    uint8_t  EntryPointChecksum;    // 入口点校验和
    uint8_t  EntryPointLength;      // 入口点结构长度
    uint8_t  SMBIOSMajorVersion;    // SMBIOS主版本
    uint8_t  SMBIOSMinorVersion;    // SMBIOS次版本
    uint8_t  SMBIOSDocrev;          // 文档修订版
    uint8_t  EntryPointRevision;    // 入口点修订版
    uint8_t  Reserved;              // 保留字节
    uint32_t StructureTableMaxSize; // 结构表最大大小
    uint64_t StructureTableAddress; // 结构表地址
};

struct EntryPoint32 {
    char     AnchorString[4];
    uint8_t  EntryPointChecksum; // 入口点校验和
    uint8_t  EntryPointLength;   // 入口点结构长度
    uint8_t  SMBIOSMajorVersion; // SMBIOS主版本
    uint8_t  SMBIOSMinorVersion; // SMBIOS次版本
    uint8_t  SMBIOSDocrev;       // 文档修订版
    uint8_t  EntryPointRevision; // 入口点修订版
    uint8_t  FormattedArea[5];   // 保留
    uint8_t  IntermediateAnchorString[5];
    uint8_t  IntermediateChecksum;
    uint16_t StructureTableMaxSize; // 结构表最大大小
    uint32_t StructureTableAddress; // 结构表地址
    uint16_t NumberOfStructures;    // 结构数量
    uint8_t  BCDRevision;           // BCD修订
};

/* 结构表头部 */
struct Header {
    uint8_t  type;   // 结构类型
    uint8_t  length; // 结构长度（不包括字符串表）
    uint16_t handle; // 结构句柄
};

typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} efi_guid_t;

/* 获取SMBIOS主版本 */
int smbios_major_version(void);

/* 获取SMBIOS次版本 */
int smbios_minor_version(void);

void smbios_setup();

/* 获取BIOS厂家字符串 */
const char *smbios_bios_vendor(void);

/* 获取BIOS版本字符串 */
const char *smbios_bios_version(void);

/* 获取BIOS起始地址段 */
uint16_t smbios_bios_starting_address_segment(void);

/* 获取BIOS发布日期字符串 */
const char *smbios_bios_release_date(void);

/* 获取BIOS ROM大小 */
uint32_t smbios_bios_rom_size(void);

/* 获取BIOS特性 */
uint64_t smbios_bios_characteristics(void);

/* 获取BIOS特性扩展字节 */
uint16_t smbios_bios_characteristics_ext(void);

/* 获取BIOS主要版本 */
uint8_t smbios_bios_major_release(void);

/* 获取BIOS次要版本 */
uint8_t smbios_bios_minor_release(void);

/* 获取BIOS嵌入式控制器固件主要版本 */
uint8_t smbios_bios_ec_major_release(void);

/* 获取BIOS嵌入式控制器固件次要版本 */
uint8_t smbios_bios_ec_minor_release(void);

/* Type-1 */

/* 获取系统制造商字符串 */
const char *smbios_sys_manufacturer(void);

/* 获取系统产品名称字符串 */
const char *smbios_sys_product_name(void);

/* 获取系统版本字符串 */
const char *smbios_sys_version(void);

/* 获取系统序列号字符串 */
const char *smbios_sys_serial_number(void);

/* 获取系统UUID */
void smbios_sys_uuid(uint8_t uuid[16]);

/* 获取系统唤醒类型 */
uint8_t smbios_sys_wakeup_type(void);

/* 获取系统SKU编号字符串 */
const char *smbios_sys_sku_number(void);

/* 获取系统系列字符串 */
const char *smbios_sys_family(void);
