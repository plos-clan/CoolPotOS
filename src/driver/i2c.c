//
// Created by Vinbe_Wan on 2024/9/16.
//

#include "../include/i2c.h"

STATIC_INLINE void IIC_SDA_OUT(void) {
    io_out32(
            GPIO_Port, (io_in32(GPIO_Port) | (0x01 << IIC_Pin))
            );
}
STATIC_INLINE void IIC_SDA_OUT_HIGH(void) {
    io_out32(
            GPIO_Port, (io_in32(GPIO_Port) | (0x01 << IIC_Pin))
    );
}
STATIC_INLINE void IIC_SDA_OUT_LOW(void) {
    io_out32(
            GPIO_Port, (io_in32(GPIO_Port) | (0x00 << IIC_Pin))
    );
}

STATIC_INLINE uint32_t IIC_SDA_IN(void) {
    return io_in32(GPIO_Port);
}

STATIC_INLINE void IIC_SCL_OUT_HIGH(void ) {
    io_out32(
            GPIO_Port, (io_in32(GPIO_Port) | (0x01 << IIC_Pin))
            );
}
STATIC_INLINE void IIC_SCL_OUT_LOW(void) {
    io_out32(
            GPIO_Port, (io_in32(GPIO_Port) | (0x00 << IIC_Pin))
            );
}

STATIC_INLINE void IIC_Delay(void)
{
    volatile uint8_t i;
    /*
		*AT32F425F6P7,
		*i = 100,SCL = 163.4KHZ,6.1us
		*i = 75, SCL = 243.9KHZ,4.1us
		*i = 50, SCL = 312.5kHZ,3.2us
    */
    for(i=0;i<75;i++);
}

void IIC_GenerateStart(void){
    IIC_SDA_OUT();
    IIC_SDA_OUT_HIGH();
    IIC_SCL_OUT_HIGH();
    IIC_Delay();
    IIC_SDA_OUT_LOW();
    IIC_Delay();
    IIC_SCL_OUT_LOW();
    IIC_Delay();
}
void IIC_GenerateStop(void){
    IIC_SDA_OUT_LOW();
    IIC_SCL_OUT_HIGH();
    IIC_Delay();
    IIC_SDA_OUT_HIGH();
    IIC_Delay();
}

