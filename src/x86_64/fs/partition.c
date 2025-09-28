#include "partition.h"
#include "device.h"
#include "errno.h"
#include "heap.h"
#include "ioctl.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "sprintf.h"
#include "vfs.h"

extern device_t device_ctl[26];
partition_t     partitions[MAX_PARTITIONS_NUM];
size_t          partition_num = 0;
partition_t    *device_lists[MAX_PARTITIONS_NUM];

size_t partition_read(int part, uint8_t *buf, size_t number, size_t lba) {
    partition_t *partition = device_lists[part];
    return device_ctl[partition->vdisk_id].read(partition->vdisk_id, buf, number,
                                                partition->starting_lba + lba);
}

size_t partition_write(int part, uint8_t *buf, size_t number, uint64_t lba) {
    partition_t *partition = device_lists[part];
    return device_ctl[partition->vdisk_id].write(partition->vdisk_id, buf, number,
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

bool parser_block_device(vfs_node_t device, device_t disk, size_t vdisk_id) {
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
                    memcpy(partition->disk_guid, gpt->disk_guid, 16);
                    memcpy(partition->partition_type_guid, entry->partition_type_guid, 16);
                    memcpy(partition->unique_partition_guid, entry->unique_partition_guid, 16);
                    memcpy(partition->partition_name, entry->partition_name, 36 * 2);
                    partition_num++;
                    char out[37];
                    format_guid(entry->unique_partition_guid, out);
                    kinfo("GPT Partition(%s) %zu GUID: %s", disk.drive_name, partition_num, out);
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
                partition->vdisk_id       = vdisk_id;
                partition->starting_lba   = starting_lba;
                partition->ending_lba     = ending_lba;
                partition->type           = MBR;
                partition->sector_size    = disk.sector_size;
                partition->is_used        = true;

                kinfo("MBR Partition(%s) %d lba=%llu..%llu %s", disk.drive_name, j, starting_lba,
                      ending_lba, (boot_sector->dpte[j].flags & 0x80) != 0 ? "bootable" : "");
                partition_num++;
            }
        }
    }
end:
    free(mbr);
    return true;
}

int extract_partition_index(const char *name) {
    const char *prefix     = "part";
    size_t      prefix_len = strlen(prefix);
    if (strncmp(name, prefix, prefix_len) != 0) { return -1; }
    const char *suffix = name + prefix_len;
    for (const char *p = suffix; *p; p++) {
        if (!isdigit((unsigned char)*p)) { return -1; }
    }
    return atoi(suffix);
}

int partition_ioctl(device_t *device, size_t req, void *arg) {
    int device_id = extract_partition_index(device->drive_name);
    if (device_id == -1) return -ENODEV;
    partition_t partition = partitions[device_id];
    switch (req) {
    case IOGPTYPE:
        if (arg == NULL) return -EINVAL;
        uint16_t *type = (uint16_t *)arg;
        *type          = partition.type == GPT   ? PARTITION_TYPE_GPT
                         : partition.type == MBR ? PARTITION_TYPE_MBR
                                                 : PARTITION_TYPE_UNKNOWN;
        break;
    case IOGPINFO:
        if (arg == NULL) return -EINVAL;
        if (partition.type == GPT) {
            struct ioctl_gpt_partition *info = (struct ioctl_gpt_partition *)arg;
            memcpy(info->disk_guid, partition.disk_guid, 16);
            memcpy(info->partition_type_guid, partition.partition_type_guid, 16);
            memcpy(info->unique_partition_guid, partition.unique_partition_guid, 16);
            memcpy(info->name, partition.partition_name, 36);
            info->size  = partition.sector_size;
            info->start = partition.starting_lba;
            info->end   = partition.ending_lba;
        }
        break;
    }
    return EOK;
}

void partition_init() {
    for (size_t i = 0; i < 26; i++) {
        if (device_ctl[i].flag >= 1 && device_ctl[i].type == DEVICE_BLOCK) {
            char buf[50];
            sprintf(buf, "/dev/%s", device_ctl[i].drive_name);
            vfs_node_t device = vfs_open(buf);
            if (device == NULL) {
                logkf("Partition init failed, device %s not found.\n", device_ctl[i].drive_name);
                continue;
            }
            if (parser_block_device(device, device_ctl[i], i)) {}
        }
    }

    kinfo("Loading part device...");

    for (size_t j = 0; j < partition_num; j++) {
        partition_t partition = partitions[j];
        device_t    part;
        part.sector_size = partition.sector_size;
        part.flag        = 1;
        part.type        = DEVICE_BLOCK;
        part.read        = partition_read;
        part.write       = partition_write;
        part.ioctl       = partition_ioctl;
        part.size = (partition.ending_lba - partition.starting_lba + 1) * partition.sector_size;
        device_t *src_dev = get_device(partition.vdisk_id);
        char      buf[20];
        sprintf(buf, "%sp%zu", src_dev->drive_name, j);
        strcpy(part.drive_name, buf);
        int id           = regist_device(NULL, part);
        device_lists[id] = &partitions[j];
    }
}
