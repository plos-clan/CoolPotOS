#include <stdint.h>

#define I2C_NAME_SIZE 32
#define STATIC_INLINE static inline

typedef struct {
    char name[I2C_NAME_SIZE];
} i2c_device_id;

typedef enum{
    I2C_BUSY = 0x01,
    I2C_NO_ACK = 0x02,
    I2C_ERROR = 0x04,
} i2c_flag;

typedef enum {
    ENABLE = 0x01u,
    DISABLE = 0x00u,
} i2c_status;

typedef struct {
    uint8_t address;
    i2c_flag flags;
    i2c_device_id i2CDeviceId;
} i2c_client;

typedef struct {
    uint16_t address;
    i2c_flag flags;
    uint16_t length;
    uint8_t  *buffer;
} i2c_message;

typedef struct {
    int (*probe)(i2c_client *, const i2c_device_id *);
    int (*remove)(i2c_client *);
} i2c_driver;

void i2c_client_set_info(i2c_client *old_client, i2c_client new_client);
void i2c_client_get_info(i2c_client *client);

STATIC_INLINE int32_t i2c_GenerateStart(i2c_client *client, i2c_status status){

};
STATIC_INLINE int32_t i2c_GenerateStop(i2c_client *client, i2c_status status){

};
STATIC_INLINE int32_t i2c_SendData(i2c_client *client, const uint8_t *buffer){

};
STATIC_INLINE int32_t i2c_ReceiveData(i2c_client *client, uint8_t *buffer){

};

int32_t i2c_read_byte(i2c_client *client, uint8_t command);
int32_t i2c_write_byte(i2c_client *client, uint8_t command, uint8_t value);
int32_t i2c_transfer(i2c_client *client, i2c_message *msg, int num);

