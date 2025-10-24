#include "driver/blk_device.h"
#include "fs/partition.h"
#include "mem/heap.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "cow_arraylist.h"
#include "errno.h"

cow_arraylist *block_device_list;

size_t blk_device_read(size_t lba, size_t number, void *buffer, blk_device_t *device){
    if(device == NULL) return -1;
    if(device->ops.read == NULL) return -1;

    size_t   total_size  = number * device->sector_size;
    size_t   page_size   = (total_size / PAGE_SIZE) == 0 ? 1 : (total_size / PAGE_SIZE);
    uint64_t phys        = alloc_frames(page_size);
    page_map_range(get_current_directory(), (uint64_t)phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, KERNEL_PTE_FLAGS);
    uint8_t *kbuf = phys_to_virt(phys);

    size_t count = device->ops.read(device->handle,(uint8_t*)kbuf,number,lba);
    if(count == -1) goto err;
    memcpy(buffer,kbuf,count);
err:
    unmap_page_range(get_current_directory(), (uint64_t)kbuf, page_size * PAGE_SIZE);
    return count;
}

size_t blk_device_write(size_t lba, size_t number, const void *buffer, blk_device_t *device){
    if(device == NULL) return -1;
    if(device->ops.write == NULL) return -1;

    size_t   total_size  = number * device->sector_size;
    size_t   page_size   = (total_size / PAGE_SIZE) == 0 ? 1 : (total_size / PAGE_SIZE);
    uint64_t phys        = alloc_frames(page_size);
    page_map_range(get_current_directory(), (uint64_t)phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, KERNEL_PTE_FLAGS);
    uint8_t *kbuf = phys_to_virt(phys);
    memcpy(kbuf,buffer,number);
    size_t count = device->ops.write(device->handle,(uint8_t*)kbuf,number,lba);
    unmap_page_range(get_current_directory(), (uint64_t)kbuf, page_size * PAGE_SIZE);
    return count;
}

errno_t delete_blk_device(size_t blk_id){
    blk_device_t *device = cow_list_get(block_device_list,blk_id);
    if(device == NULL) return -ENODEV;
    errno_t res = EOK;
    if(device->ops.del_blk != NULL) res = device->ops.del_blk(device->handle);
    free(device);
    return res;
}

size_t register_device(blk_device_t *device){
    if(device == NULL || device->handle == NULL) return -ENODEV;
    device->device_id = cow_list_add(block_device_list,device);
    if(device->type == BLK_BLOCK_DEVICE){
        parser_block_device(device);
    }
    return device->device_id;
}

void init_block_device_manager() {
    block_device_list = cow_list_create();
}
