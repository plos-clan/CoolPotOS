#include "fs/partition.h"
#include "driver/blk_device.h"
#include "fs/vfs.h"
#include "lib/sprintf.h"
#include "term/klog.h"

partition_t partitions[MAX_PARTITIONS_NUM];
size_t      partition_num = 0;

void format_guid(const uint8_t guid[16], char out[37]) {
    snprintf(out, 37, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             guid[3], guid[2], guid[1], guid[0],                        // time_low
             guid[5], guid[4],                                          // time_mid
             guid[7], guid[6],                                          // time_hi_and_version
             guid[8], guid[9],                                          // clock_seq
             guid[10], guid[11], guid[12], guid[13], guid[14], guid[15] // node
    );
}

bool is_partition_used(struct GPT_DPTE *entry) {
    if (entry->starting_lba == 0 || entry->ending_lba == 0) return false;

    const uint8_t *partition_type_guid = entry->unique_partition_guid;
    for (int i = 0; i < 16; ++i) {
        if (partition_type_guid[i] != 0) return true;
    }
    return false;
}

size_t partition_read(void *handle, uint8_t *buf, size_t number, size_t lba) {
    partition_t *partition = handle;
    return partition->device->ops.read(partition->device, buf, number,
                                       partition->starting_lba + lba);
}

size_t partition_write(void *handle, uint8_t *buf, size_t number, uint64_t lba) {
    partition_t *partition = handle;
    return partition->device->ops.write(partition->device, buf, number,
                                        partition->starting_lba + lba);
}

static bool parse_gpt_partitions(blk_device_t *disk, struct GPT_DPT *gpt) {
    if (memcmp(gpt->signature, "EFI PART", 8) != 0 ||
        gpt->num_partition_entries == 0 ||
        gpt->partition_entry_lba == 0 ||
        gpt->size_of_partition_entry < sizeof(struct GPT_DPTE) ||
        gpt->size_of_partition_entry > 512) {
        return false;
    }

    size_t dptes_size = (size_t)gpt->num_partition_entries * gpt->size_of_partition_entry;
    struct GPT_DPTE *dptes = (struct GPT_DPTE *)malloc(dptes_size);
    if (!dptes) return false;

    if (blk_device_read(disk, (uint8_t *)dptes,
                        gpt->partition_entry_lba * disk->block_size,
                        dptes_size) == (size_t)-1) {
        free(dptes);
        return false;
    }

    for (size_t j = 0; j < gpt->num_partition_entries; j++) {
        struct GPT_DPTE *entry =
            (struct GPT_DPTE *)((uint8_t *)dptes + j * gpt->size_of_partition_entry);
        if (is_partition_used(entry)) {
            if (partition_num >= MAX_PARTITIONS_NUM) {
                kwarn("Too many partitions, ignoring extra ones");
                break;
            }

            partition_t *partition = &partitions[partition_num];
            partition->device = disk;
            partition->starting_lba = entry->starting_lba;
            partition->ending_lba = entry->ending_lba;
            partition->type = GPT;
            partition->sector_size = disk->block_size;
            partition->is_used = true;
            memcpy(partition->disk_guid, gpt->disk_guid, 16);
            memcpy(partition->partition_type_guid, entry->partition_type_guid, 16);
            memcpy(partition->unique_partition_guid, entry->unique_partition_guid, 16);
            memcpy(partition->partition_name, entry->partition_name, 36 * 2);

            char guid_str[37];
            format_guid(entry->unique_partition_guid, guid_str);
            kinfo("GPT Partition(%s) %zu GUID: %s", disk->name, partition_num + 1, guid_str);

            // 注册为块设备
            blk_device_t *part = (blk_device_t *)malloc(sizeof(blk_device_t));
            if (part) {
                part->size = (partition->ending_lba - partition->starting_lba + 1) * partition->sector_size;
                part->block_size = partition->sector_size;
                part->ops.read = partition_read;
                part->ops.write = partition_write;
                part->ops.ioctl = (void *)dummy;
                part->ops.poll = (void *)dummy;
                part->ops.map = (void *)dummy;
                part->handle = partition;
                part->type = BLK_PARTITION;
                register_device(part);
            }

            partition_num++;
        }
    }

    free(dptes);
    return true;
}

// 从保护性 MBR 中解析 GPT（LBA 由 MBR 指定）
static bool parse_gpt_from_protective_mbr(blk_device_t *disk, uint8_t *mbr) {
    uint32_t gpt_lba_start = *(uint32_t *)&mbr[0x1BE + 8];
    struct GPT_DPT *gpt = (struct GPT_DPT *)malloc(disk->block_size);
    if (!gpt) return false;

    if (blk_device_read(disk, (uint8_t *)gpt,
                        (uint64_t)gpt_lba_start * disk->block_size,
                        disk->block_size) == (size_t)-1) {
        free(gpt);
        return false;
    }

    bool ok = parse_gpt_partitions(disk, gpt);
    free(gpt);
    return ok;
}

// 直接尝试从 LBA1 读取 GPT（用于 ISO / raw GPT 映像）
static bool try_gpt_at_lba1(blk_device_t *disk) {
    struct GPT_DPT *gpt = (struct GPT_DPT *)malloc(disk->block_size);
    if (!gpt) return false;

    if (blk_device_read(disk, (uint8_t *)gpt, disk->block_size, disk->block_size) == (size_t)-1) {
        free(gpt);
        return false;
    }

    bool is_valid = (memcmp(gpt->signature, "EFI PART", 8) == 0);
    if (is_valid) {
        kinfo("Detected GPT at LBA1 (e.g., hybrid ISO)");
        bool ok = parse_gpt_partitions(disk, gpt);
        free(gpt);
        return ok;
    }

    free(gpt);
    return false;
}

bool parser_block_device(blk_device_t *disk) {
    uint8_t *mbr = (uint8_t *)calloc(1, disk->block_size);
    if (!mbr) return false;

    if (blk_device_read(disk, mbr, 0, disk->block_size) == (size_t)-1) {
        free(mbr);
        return false;
    }

    if (mbr[0x1FE] == 0x55 && mbr[0x1FF] == 0xAA) {
        uint8_t part_type = mbr[0x1BE + 4]; // 第一个分区类型

        // 保护性 MBR（GPT）
        if (part_type == 0xEE) {
            bool gpt_ok = parse_gpt_from_protective_mbr(disk, mbr);
            free(mbr);
            return gpt_ok;
        }

        // 普通 MBR
        struct MBR_DPT *boot_sector = (struct MBR_DPT *)mbr;
        for (int j = 0; j < MBR_MAX_PARTITION_NUM; j++) {
            if (boot_sector->dpte[j].start_lba == 0 || boot_sector->dpte[j].sectors_limit == 0)
                continue;

            if (partition_num >= MAX_PARTITIONS_NUM) break;

            partition_t *partition = &partitions[partition_num];
            partition->device = disk;
            partition->starting_lba = boot_sector->dpte[j].start_lba;
            partition->ending_lba = boot_sector->dpte[j].start_lba + boot_sector->dpte[j].sectors_limit - 1;
            partition->type = MBR;
            partition->sector_size = disk->block_size;
            partition->is_used = true;

            kinfo("MBR Partition(%s) %d lba=%llu..%llu %s", disk->name, j,
                  partition->starting_lba, partition->ending_lba,
                  (boot_sector->dpte[j].flags & 0x80) ? "bootable" : "");

            blk_device_t *part = (blk_device_t *)malloc(sizeof(blk_device_t));
            if (part) {
                part->size = (partition->ending_lba - partition->starting_lba + 1) * partition->sector_size;
                part->block_size = partition->sector_size;
                part->ops.read = partition_read;
                part->ops.write = partition_write;
                part->ops.ioctl = (void *)dummy;
                part->ops.poll = (void *)dummy;
                part->ops.map = (void *)dummy;
                part->handle = partition;
                part->type = BLK_PARTITION;
                register_device(part);
            }
            partition_num++;
        }
        free(mbr);
        return true;
    }

    free(mbr);
    if (try_gpt_at_lba1(disk)) {
        return true;
    }

    kinfo("device raw partition: %s", disk->name);
    return true;
}
