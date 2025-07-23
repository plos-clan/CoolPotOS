#pragma once

#define GPT_HEADER_SIGNATURE "EFI PART"
#define MAX_PARTITIONS_NUM   128

#define PARTITION_TYPE_GPT     0xC12A7328
#define PARTITION_TYPE_MBR     0xEBD0A0A2
#define PARTITION_TYPE_UNKNOWN 0xFFFFFFFF

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

struct GPT_DPTE {
    uint8_t  partition_type_guid[16];   // Partition type GUID
    uint8_t  unique_partition_guid[16]; // Unique partition GUID
    uint64_t starting_lba;              // Starting LBA of the partition
    uint64_t ending_lba;                // Ending LBA of the partition
    uint64_t attributes;                // Partition attributes
    uint16_t partition_name[36];        // Partition name (UTF-16LE, null-terminated)
} __attribute__((packed));

typedef struct partition {
    size_t vdisk_id;
    size_t starting_lba;
    size_t ending_lba;
    size_t sector_size;
    enum {
        MBR = 1,
        GPT,
        PRAW,
    } type;
    bool     is_used;
    uint16_t partition_name[36];
    uint8_t  partition_type_guid[16];   // 分区类型 GUID
    uint8_t  unique_partition_guid[16]; // 分区 GUID
    uint8_t  disk_guid[16];             // 设备 GUID
} partition_t;

void partition_init();
