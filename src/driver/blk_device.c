#include "driver/blk_device.h"
#include "cow_arraylist.h"
#include "driver/ioctl.h"
#include "errno.h"
#include "fs/partition.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"
#include "term/klog.h"

cow_arraylist *block_device_list;

size_t blk_device_read(blk_device_t *device, void *buffer, size_t offset, size_t length) {
    if (device == NULL) return -1;
    if (device->ops.read == NULL) return -1;

    if (device->type == BLK_STREAM_DEVICE) {
        return device->ops.read(device->handle, buffer, offset, length);
    }

    uint64_t start_sector    = offset / device->block_size;
    uint64_t end_sector      = (offset + length - 1) / device->block_size;
    uint64_t sector_count    = end_sector - start_sector + 1;
    uint64_t offset_in_block = offset % device->block_size;

    size_t   total_size = sector_count * device->block_size;
    size_t   page_size  = (total_size / PAGE_SIZE) == 0 ? 1 : (total_size / PAGE_SIZE);
    uint64_t phys       = alloc_frames(page_size);
    page_map_range(get_current_directory(), (uint64_t)driver_phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, KERNEL_PTE_FLAGS);
    uint8_t *kbuf = driver_phys_to_virt(phys);

    if ((offset_in_block == 0) && ((length % device->block_size) == 0)) {
        uint64_t total_copied      = 0;
        uint64_t remaining_sectors = sector_count;
        while (remaining_sectors > 0) {
            uint64_t to_copy_sectors =
                MIN(remaining_sectors, device->max_size / device->block_size);

            size_t read_length = start_sector + total_copied / device->block_size;
            read_length        = read_length == 0 ? device->block_size : read_length;

            device->ops.read(device->handle, kbuf, to_copy_sectors, read_length);
            uint64_t to_copy_bytes = to_copy_sectors * device->block_size;
            memcpy(buffer + total_copied, kbuf, to_copy_bytes);
            total_copied      += to_copy_bytes;
            remaining_sectors -= to_copy_sectors;
        }
        unmap_page_range(get_kernel_pagedir(), (uint64_t)kbuf, sector_count * device->block_size);
        return total_copied;
    }

    uint64_t total_read = 0;
    uint64_t remaining  = length;
    uint8_t *dest       = (uint8_t *)buffer;

    while (remaining > 0) {
        // 计算本次操作的扇区数和长度
        uint64_t chunk_sectors = sector_count;
        uint64_t chunk_size    = remaining;

        // 限制单次I/O大小
        if (chunk_sectors * device->block_size > device->max_size) {
            chunk_sectors = device->max_size / device->block_size;
            chunk_size    = chunk_sectors * device->block_size - offset_in_block;
            if (chunk_size > remaining) { chunk_size = remaining; }
        }

        // 执行块设备读取
        if (device->ops.read(device->handle, kbuf, chunk_sectors, start_sector) != chunk_sectors) {
            printk("Read block device failed!!!\n");
            unmap_page_range(get_kernel_pagedir(), (uint64_t)kbuf,
                             sector_count * device->block_size);
            return (uint64_t)-1;
        }

        // 复制数据到目标缓冲区
        uint64_t copy_size = (chunk_size > remaining) ? remaining : chunk_size;

        memcpy(dest, kbuf + offset_in_block, copy_size);

        // 更新状态
        dest            += copy_size;
        remaining       -= copy_size;
        total_read      += copy_size;
        start_sector    += chunk_sectors;
        offset_in_block  = 0; // 第一次之后不需要再处理块内偏移
    }

    unmap_page_range(get_kernel_pagedir(), (uint64_t)kbuf, sector_count * device->block_size);

    return total_read;
}

size_t blk_device_write(blk_device_t *device, const void *buffer, size_t offset, size_t length) {
    if (device == NULL) return -1;
    if (device->ops.write == NULL) return -1;

    if (device->type == BLK_STREAM_DEVICE) {
        return device->ops.write(device->handle, (uint8_t *)buffer, offset, length);
    }

    uint64_t start_sector    = offset / device->block_size;
    uint64_t end_sector      = (offset + length - 1) / device->block_size;
    uint64_t sector_count    = end_sector - start_sector + 1;
    uint64_t offset_in_block = offset % device->block_size;

    size_t   total_size = sector_count * device->block_size;
    size_t   page_size  = (total_size / PAGE_SIZE) == 0 ? 1 : (total_size / PAGE_SIZE);
    uint64_t phys       = alloc_frames(page_size);
    page_map_range(get_current_directory(), (uint64_t)driver_phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, KERNEL_PTE_FLAGS);
    uint8_t *tmp = driver_phys_to_virt(phys);

    if ((offset_in_block == 0) && ((length % device->block_size) == 0)) {
        uint64_t total_copied      = 0;
        uint64_t remaining_sectors = sector_count;
        while (remaining_sectors > 0) {
            uint64_t to_copy_sectors =
                MIN(remaining_sectors, device->max_size / device->block_size);
            uint64_t to_copy_bytes = to_copy_sectors * device->block_size;
            memcpy(tmp, buffer + total_copied, to_copy_bytes);

            size_t write_length = start_sector + total_copied / device->block_size;
            write_length        = write_length == 0 ? device->block_size : write_length;

            device->ops.write(device->handle, tmp, write_length, to_copy_sectors);
            total_copied      += to_copy_bytes;
            remaining_sectors -= to_copy_sectors;
        }
        unmap_page_range(get_kernel_pagedir(), (uint64_t)tmp, sector_count * device->block_size);
        return total_copied;
    }

    uint64_t       total_written = 0;
    uint64_t       remaining     = length;
    const uint8_t *src           = (const uint8_t *)buffer;

    while (remaining > 0) {
        // 计算本次操作的扇区数和长度
        uint64_t chunk_sectors = sector_count;
        uint64_t chunk_size    = remaining;

        // 限制单次I/O大小
        if (chunk_sectors * device->block_size > device->max_size) {
            chunk_sectors = device->max_size / device->block_size;
            chunk_size    = chunk_sectors * device->block_size - offset_in_block;
            if (chunk_size > remaining) { chunk_size = remaining; }
        }

        // 对于部分块写入，需要先读取原始数据
        if (offset_in_block != 0 || chunk_size < chunk_sectors * device->block_size) {
            if (device->ops.read(device->handle, tmp, chunk_sectors, start_sector) !=
                chunk_sectors) {
                printk("Read block device failed!!!\n");
                unmap_page_range(get_kernel_pagedir(), (uint64_t)tmp,
                                 sector_count * device->block_size);
                return (uint64_t)-1;
            }
        }

        // 复制数据到临时缓冲区
        uint64_t copy_size = (chunk_size > remaining) ? remaining : chunk_size;

        memcpy(tmp + offset_in_block, src, copy_size);

        // 执行块设备写入
        if (device->ops.write(device->handle, tmp, chunk_sectors, start_sector) != chunk_sectors) {
            printk("Write block device failed!!!\n");
            unmap_page_range(get_kernel_pagedir(), (uint64_t)tmp,
                             sector_count * device->block_size);
            return (uint64_t)-1;
        }

        // 更新状态
        src             += copy_size;
        remaining       -= copy_size;
        total_written   += copy_size;
        start_sector    += chunk_sectors;
        offset_in_block  = 0; // 第一次之后不需要再处理块内偏移
    }

    unmap_page_range(get_kernel_pagedir(), (uint64_t)tmp, sector_count * device->block_size);

    return total_written;
}

size_t blk_size_t(blk_device_t *device) {
    return device->size;
}

errno_t blk_ioctl(blk_device_t *device, size_t cmd, void *arg) {
    switch (cmd) {
    case BLKGETSIZE64: *(uint64_t *)arg = device->size; break;
    case BLKGETSIZE: *(unsigned long *)arg = device->size / device->block_size; break;
    case BLKSSZGET: *((int *)arg) = device->block_size; break;
    case BLKRRPART:
        if (device->type != BLK_BLOCK_DEVICE) return -ENOSYS;
        parser_block_device(device);
        break;
    default: return -ENOSYS;
    }
    return EOK;
}

errno_t blk_poll(blk_device_t *device, size_t events) {
    return events;
}

errno_t delete_blk_device(size_t blk_id) {
    blk_device_t *device = cow_list_get(block_device_list, blk_id);
    if (device == NULL) return -ENODEV;
    errno_t res = EOK;
    if (device->ops.del_blk != NULL) res = device->ops.del_blk(device->handle);
    free(device);
    return res;
}

size_t register_device(blk_device_t *device) {
    if (device == NULL || device->handle == NULL) return -ENODEV;
    device->device_id = cow_list_add(block_device_list, device);
    if (device->type == BLK_BLOCK_DEVICE) { parser_block_device(device); }
    return device->device_id;
}

void init_block_device_manager() {
    block_device_list = cow_list_create();
}
