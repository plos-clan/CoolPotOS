#include "iic.h"
#include "klog.h"

int crc_check(IIC_Data *frame) {
    uint32_t *data = frame->data;
    uint8_t crc = crc8(data, frame->data_len);
    if (crc != frame->crc) {
        printk("CRC error!\n");
        return 0xFFFFFFFF;
    }else{
        // 将原始数据转换为32位值
        uint32_t original_data = 0;
        for (uint8_t i = 0; i < frame->data_len; i++) {
            original_data |= (uint32_t)
            data[i] << (8 * i);
        }
        return original_data;
    }
};