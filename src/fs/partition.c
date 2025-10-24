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

bool parser_block_device(blk_device_t *disk) {
    uint8_t *mbr = malloc(disk->sector_size);
    if (blk_device_read(0, disk->sector_size,mbr, disk) == (size_t)-1) return false;
    if (mbr[0x1FE] == 0x55 && mbr[0x1FF] == 0xAA) {
        uint8_t part_type = mbr[0x1BE + 4];
        if (part_type == 0xEE) {
            struct GPT_DPT *gpt = malloc(disk->sector_size);
            if (disk->ops.read(disk, (uint8_t *)gpt, 1 * disk->sector_size, disk->sector_size) ==
                (size_t)-1) {
                free(gpt);
                free(mbr);
                return false;
            }
            if (memcmp(gpt->signature, GPT_HEADER_SIGNATURE, 8) ||
                gpt->num_partition_entries == 0 || gpt->partition_entry_lba == 0) {
                free(gpt);
                return false;
            }
            char disk_uid[37];
            format_guid(gpt->disk_guid, disk_uid);

            size_t           dptes_size = gpt->num_partition_entries * gpt->size_of_partition_entry;
            struct GPT_DPTE *dptes      = (struct GPT_DPTE *)malloc(dptes_size);
            blk_device_read(gpt->partition_entry_lba, dptes_size,dptes,disk);
            for (size_t j = 0; j < gpt->num_partition_entries; j++) {
                struct GPT_DPTE *entry =
                    (struct GPT_DPTE *)((uint8_t *)dptes + j * gpt->size_of_partition_entry);
                if (is_partition_used(entry)) {
                    partition_t *partition  = &partitions[partition_num];
                    partition->device       = disk;
                    partition->starting_lba = entry->starting_lba;
                    partition->ending_lba   = entry->ending_lba;
                    partition->type         = GPT;
                    partition->sector_size  = disk->sector_size;
                    partition->is_used      = true;
                    memcpy(partition->disk_guid, gpt->disk_guid, 16);
                    memcpy(partition->partition_type_guid, entry->partition_type_guid, 16);
                    memcpy(partition->unique_partition_guid, entry->unique_partition_guid, 16);
                    memcpy(partition->partition_name, entry->partition_name, 36 * 2);
                    partition_num++;
                    char out[37];
                    format_guid(entry->unique_partition_guid, out);
                    kinfo("GPT Partition(%s) %zu GUID: %s", disk->name, partition_num, out);
                    blk_device_t *part = malloc(sizeof(blk_device_t));
                    part->size =
                        (partition->ending_lba - partition->starting_lba) * partition->sector_size;
                    part->sector_size = partition->sector_size;
                    part->ops.read    = partition_read;
                    part->ops.write   = partition_write;
                    part->handle      = partition;
                    part->ops.ioctl   = (void *)dummy;
                    part->ops.poll    = (void *)dummy;
                    part->ops.map     = (void *)dummy;
                    part->type        = BLK_PARTITION;
                    register_device(part);
                }
            }
            free(dptes);
            free(gpt);
        } else {
            struct MBR_DPT *boot_sector = (struct MBR_DPT *)mbr;
            if (boot_sector->bs_trail_sig != 0xAA55) { goto end; }
            for (int j = 0; j < MBR_MAX_PARTITION_NUM; j++) {
                if (boot_sector->dpte[j].start_lba == 0 || boot_sector->dpte[j].sectors_limit == 0)
                    continue;
                size_t       starting_lba = boot_sector->dpte[j].start_lba;
                size_t       ending_lba   = boot_sector->dpte[j].sectors_limit;
                partition_t *partition    = &partitions[partition_num];
                partition->device         = disk;
                partition->starting_lba   = starting_lba;
                partition->ending_lba     = ending_lba;
                partition->type           = MBR;
                partition->sector_size    = disk->sector_size;
                partition->is_used        = true;
                kinfo("MBR Partition(%s) %d lba=%llu..%llu %s", disk->name, j, starting_lba,
                      ending_lba, (boot_sector->dpte[j].flags & 0x80) != 0 ? "bootable" : "");
                partition_num++;

                blk_device_t *part = malloc(sizeof(blk_device_t));
                part->size =
                    (partition->ending_lba - partition->starting_lba) * partition->sector_size;
                part->sector_size = partition->sector_size;
                part->ops.read    = partition_read;
                part->ops.write   = partition_write;
                part->handle      = partition;
                part->ops.ioctl   = (void *)dummy;
                part->ops.poll    = (void *)dummy;
                part->ops.map     = (void *)dummy;
                part->type        = BLK_PARTITION;
                register_device(part);
            }
        }
    } else
        kinfo("device raw partition: %s", disk->name);
end:
    free(mbr);
    return true;
}
