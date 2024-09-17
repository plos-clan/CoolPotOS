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

typedef struct {
    char name[I2C_NAME_SIZE];
} iic_device_id;
typedef struct {
    uint16_t length;
    uint8_t  *buffer;
} iic_message;

typedef struct {
    uint8_t address;
    iic_device_id iiCDeviceId;
    List iic_slave_list;
} iic_slave_node;
typedef struct {
    int (*probe)(iic_slave_node *, const iic_device_id *);
    int (*remove)(iic_slave_node *);
} iic_driver;

void IIC_client_set_info(iic_slave_node *old_client, iic_slave_node new_client);
void IIC_client_get_info(iic_slave_node *client);

void IIC_GenerateStart(void);
void IIC_GenerateStop(void);

void IIC_Acknowledge(void);
void IIC_NoAcknowledge(void);
bool IIC_ReceiveAcknowledge(void);

void IIC_SendByte(uint8_t byte);
int32_t IIC_ReceiveByte(void);

bool IIC_SendWriteAddress(iic_slave_node *);
bool IIC_SendReadAddress(iic_slave_node *);

bool IIC_CheckClient(iic_slave_node *client);

#endif // CRASHPOWEROS_I2C_H