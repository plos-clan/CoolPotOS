#ifndef CRASHPOWEROS_I2C_H
#define CRASHPOWEROS_I2C_H


#include <stdint.h>
#include <stdbool.h>
#include <list.h>
#include <io.h>

#define I2C_NAME_SIZE 32
#define STATIC_INLINE static inline
#define GPIO_Port 0x12345678    // TODO: change to actual port
#define IIC_Pin 0x01    // TODO: change to actual pin

typedef struct {
    char name[I2C_NAME_SIZE];
} iic_device_id;
typedef enum{
    I2C_BUSY = 0x01,
    I2C_NO_ACK = 0x02,
    I2C_ERROR = 0x04,
} iic_flag;
typedef enum {
    ENABLE = 0x01u,
    DISABLE = 0x00u,
} iic_status;
typedef struct {
    uint8_t address;
    iic_flag flags;
    iic_device_id i2CDeviceId;
} iic_client;
typedef struct {
    uint16_t address;
    iic_flag flags;
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
bool IIC_SendAddress(iic_client *client);

STATIC_INLINE int32_t IIC_SendByte(iic_client *client, const uint8_t *buffer){

};
STATIC_INLINE int32_t IIC_ReceiveByte(iic_client *client, uint8_t *buffer){

};

int32_t IIC_read_data(iic_client *client, uint8_t command);
int32_t IIC_write_data(iic_client *client, uint8_t command, uint8_t value);
int32_t IIC_transfer(iic_client *client, iic_message *msg, int num);

#endif // CRASHPOWEROS_I2C_H