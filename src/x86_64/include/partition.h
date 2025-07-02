#pragma once

#include "ctype.h"

struct GPT_DPT {
    char     signature[8];                // 签名，必须是 "EFI PART"
    uint32_t revision;                    // 修订版本，通常为 0x00010000
    uint32_t header_size;                 // 头部大小，通常为 92 字节
    uint32_t header_crc32;                // 头部CRC32校验值
    uint32_t reserved;                    // 保留字段，必须为 0
    uint64_t my_lba;                      // 当前LBA（这个头部所在的LBA）
    uint64_t alternate_lba;               // 备份头部所在的LBA
    uint64_t first_usable_lba;            // 第一个可用的LBA（用于分区）
    uint64_t last_usable_lba;             // 最后一个可用的LBA（用于分区）
    uint8_t  disk_guid[16];               // 磁盘的GUID
    uint64_t partition_entry_lba;         // 分区表项的起始LBA
    uint32_t num_partition_entries;       // 分区表项的数量
    uint32_t size_of_partition_entry;     // 每个分区表项的大小（通常为 128 字节）
    uint32_t partition_entry_array_crc32; // 分区表项的CRC32校验值
} __attribute__((packed));
