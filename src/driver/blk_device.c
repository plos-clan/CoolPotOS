#include "driver/blk_device.h"
#include "mem/heap.h"
#include "cow_arraylist.h"
#include "errno.h"

cow_arraylist *block_device_list;

size_t device_read(size_t lba, size_t number, void *buffer, size_t blk_id){
    blk_device_t *device = cow_list_get(block_device_list,blk_id);
    if(device == NULL) return -ENODEV;
    if(device->ops.read == NULL) return -ENODEV;
    return device->ops.read(device->handle,buffer,number,lba);
}

size_t device_write(size_t lba, size_t number, const void *buffer, size_t blk_id){
    blk_device_t *device = cow_list_get(block_device_list,blk_id);
    if(device == NULL) return -ENODEV;
    if(device->ops.write == NULL) return -ENODEV;
    return device->ops.write(device->handle,(uint8_t*)buffer,number,lba);
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
    if(device == NULL || device->handle) return -ENODEV;
    device->device_id = cow_list_add(block_device_list,device);
    return device->device_id;
}

void init_block_device_manager() {
    block_device_list = cow_list_create();
}
