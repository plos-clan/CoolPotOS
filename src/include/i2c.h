#include <stdint.h>

#define I2C_NAME_SIZE 32

typedef struct {
    char name[I2C_NAME_SIZE];
} i2c_device_id;

typedef struct {
    uint8_t address;
    uint8_t flags;
    i2c_device_id i2CDeviceId;
} i2c_client;

typedef struct {
    uint16_t address;
    uint16_t flags;
    uint16_t length;
    uint8_t  *buffer;
} i2c_message;

typedef struct {
    int (*probe)(i2c_client *, const i2c_device_id *);
    int (*remove)(i2c_client *);
} i2c_driver;

void i2c_client_set_info(i2c_client *old_client, i2c_client new_client);

void i2c_client_get_info(i2c_client *client);

int32_t i2c_master_send(i2c_client *client, const uint8_t *buffer);

int32_t i2c_master_receive(i2c_client *client, uint8_t *buffer);

int32_t i2c_read_byte(i2c_client *client, uint8_t command);

int32_t i2c_write_byte(i2c_client *client, uint8_t command, uint8_t value);

int32_t i2c_transfer(i2c_client *client, i2c_message *msg, int num);

