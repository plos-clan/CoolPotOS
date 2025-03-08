// smbios.h
// 系统管理BIOS头文件
// 2025/3/8 By MicroFish
// GPL-3.0 License

#pragma once
#include "stdint.h"

/* 入口点结构 */
struct EntryPoint64 {
	char AnchorString[5];			// 锚字符串，64位为"_SM3_"
	uint8_t EntryPointChecksum;		// 入口点校验和
	uint8_t EntryPointLength;		// 入口点结构长度
	uint8_t SMBIOSMajorVersion;		// SMBIOS主版本
	uint8_t SMBIOSMinorVersion;		// SMBIOS次版本
	uint8_t SMBIOSDocrev;			// 文档修订版
	uint8_t EntryPointRevision;		// 入口点修订版
	uint8_t Reserved;				// 保留字节
	uint32_t StructureTableMaxSize;	// 结构表最大大小
	uint64_t StructureTableAddress;	// 结构表地址
};

/* 结构表头部 */
struct Header {
	uint8_t type;		// 结构类型
	uint8_t length;		// 结构长度（不包括字符串表）
	uint16_t handle;	// 结构句柄
};

/* 获取SMBIOS主版本 */
int smbios_major_version(void);

/* 获取SMBIOS次版本 */
int smbios_minor_version(void);
