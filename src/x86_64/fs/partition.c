#include "partition.h"
#include "devfs.h"
#include "heap.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "sprintf.h"
#include "vdisk.h"
#include "vfs.h"

extern vdisk vdisk_ctl[26];
partition_t  partitions[MAX_PARTITIONS_NUM];
size_t       partition_num = 0;
partition_t *device_lists[MAX_PARTITIONS_NUM];

size_t partition_read(int part, uint8_t *buf, size_t number, size_t lba) {
    partition_t *partition = device_lists[part];
    return vdisk_ctl[partition->vdisk_id].read(partition->vdisk_id, buf, number,
                                               partition->starting_lba + lba);
}

size_t partition_write(int part, uint8_t *buf, size_t number, uint64_t lba) {
    partition_t *partition = device_lists[part];
    return vdisk_ctl[partition->vdisk_id].write(partition->vdisk_id, buf, number,
                                                partition->starting_lba + lba);
}

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

bool parser_block_device(vfs_node_t device, vdisk disk, size_t vdisk_id) {
    uint8_t *mbr = malloc(disk.sector_size);
    if (vfs_read(device, mbr, 0, disk.sector_size) == (size_t)VFS_STATUS_FAILED) return false;
    if (mbr[0x1FE] == 0x55 && mbr[0x1FF] == 0xAA) {
        uint8_t part_type = mbr[0x1BE + 4];
        if (part_type == 0xEE) {
            struct GPT_DPT *gpt = malloc(disk.sector_size);
            if (vfs_read(device, gpt, 1 * disk.sector_size, disk.sector_size) ==
                (size_t)VFS_STATUS_FAILED) {
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
            vfs_read(device, dptes, gpt->partition_entry_lba * disk.sector_size, dptes_size);
            for (size_t j = 0; j < gpt->num_partition_entries; j++) {
                struct GPT_DPTE *entry =
                    (struct GPT_DPTE *)((uint8_t *)dptes + j * gpt->size_of_partition_entry);
                if (is_partition_used(entry)) {
                    partition_t *partition  = &partitions[partition_num];
                    partition->vdisk_id     = vdisk_id;
                    partition->starting_lba = entry->starting_lba;
                    partition->ending_lba   = entry->ending_lba;
                    partition->type         = GPT;
                    partition->sector_size  = disk.sector_size;
                    partition->is_used      = true;
                    memcpy(partition->partition_name, entry->partition_name, 36 * 2);
                    partition_num++;
                    char out[37];
                    format_guid(entry->unique_partition_guid, out);
                    kinfo("GPT Partition(%s) %zu GUID: %s", disk.drive_name, partition_num, out);
                } else
                    logkf("Empty GPT partition\r\n");
            }
            free(dptes);
            free(gpt);
        } else {
            logkf("MBR detected on %s.\n", disk.drive_name);
        }
    }
    free(mbr);
    return true;
}

void partition_init() {
    for (size_t i = 0; i < 26; i++) {
        if (vdisk_ctl[i].flag >= 1 && vdisk_ctl[i].type == VDISK_BLOCK) {
            char buf[50];
            sprintf(buf, "/dev/%s", vdisk_ctl[i].drive_name);
            vfs_node_t device = vfs_open(buf);
            if (device == NULL) {
                logkf("Partition init failed, device %s not found.\n", vdisk_ctl[i].drive_name);
                continue;
            }
            if (parser_block_device(device, vdisk_ctl[i], i)) {}
        }
    }

    kinfo("Loading part device...");

    for (size_t j = 0; j < partition_num; j++) {
        partition_t partition = partitions[j];
        vdisk       part;
        part.sector_size = partition.sector_size;
        part.flag        = 1;
        part.type        = VDISK_BLOCK;
        part.read        = partition_read;
        part.write       = partition_write;
        part.size = (partition.ending_lba - partition.starting_lba + 1) * partition.sector_size;
        char buf[20];
        sprintf(buf, "part%zu", j);
        strcpy(part.drive_name, buf);
        int id           = regist_vdisk(part);
        device_lists[id] = &partitions[j];
    }
}
