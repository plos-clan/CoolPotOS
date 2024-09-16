#ifndef CRASHPOWEROS_I2C_H
#define CRASHPOWEROS_I2C_H


#include <stdint.h>
#include <stdbool.h>
#include "../include/list.h"
#include "../include/io.h"

#define I2C_NAME_SIZE 32
#define STATIC_INLINE static inline
#define GPIO_Port 0x12345678    // TODO: change to actual port
#define IIC_Pin 0x01    // TODO: change to actual pin

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

typedef struct {
    char name[I2C_NAME_SIZE];
} iic_device_id;
typedef struct {
    uint8_t address;
    iic_device_id i2CDeviceId;
} iic_client;
typedef struct {
    uint16_t length;
    uint8_t  *buffer;
} iic_message;

typedef struct {
    int (*probe)(iic_client *, const iic_device_id *);
    int (*remove)(iic_client *);
} iic_driver;

void IIC_client_set_info(iic_client *old_client, iic_client new_client);
void IIC_client_get_info(iic_client *client);

void IIC_GenerateStart(void);
void IIC_GenerateStop(void);

void IIC_Acknowledge(void);
void IIC_NoAcknowledge(void);
bool IIC_ReceiveAcknowledge(void);

void IIC_SendByte(uint8_t byte);
int32_t IIC_ReceiveByte(void);

bool IIC_SendWriteAddress(iic_client *);
bool IIC_SendReadAddress(iic_client *);

bool IIC_CheckClient(iic_client *client);

#endif // CRASHPOWEROS_I2C_H