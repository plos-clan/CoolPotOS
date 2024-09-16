//
// Created by Vinbe_Wan on 2024/9/16.
//

#include "../include/i2c.h"

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
    IIC_SDA_OUT();
    IIC_SDA_OUT_LOW();
    IIC_SCL_OUT_HIGH();
    IIC_Delay();
    IIC_SDA_OUT_HIGH();
    IIC_Delay();
}

void IIC_Acknowledge(void){
    IIC_SDA_OUT();
    IIC_SDA_OUT_LOW();
    IIC_Delay();
    IIC_SCL_OUT_HIGH();
    IIC_Delay();
    IIC_SCL_OUT_LOW();
    IIC_Delay();
    IIC_SDA_OUT_HIGH();

}
void IIC_NoAcknowledge(void){
    IIC_SDA_OUT();
    IIC_SDA_OUT_LOW();
    IIC_SCL_OUT_HIGH();
    IIC_Delay();
    IIC_SDA_OUT_HIGH();
}
bool IIC_ReceiveAcknowledge(void){
    uint8_t temp;

    IIC_SDA_OUT();
    IIC_SDA_OUT_HIGH();
    IIC_Delay();
    IIC_SCL_OUT_HIGH();
    IIC_Delay();
    temp = IIC_SDA_IN();
    IIC_SCL_OUT_LOW();
    IIC_Delay();
    if(temp != 0) {
        return 1;
    } else {
        return 0;
    }
}

void IIC_SendByte(uint8_t byte){
    uint8_t i;
    for ( i = 0; i < 8; i++)
    {
        if (byte & 0x80)
        {
            IIC_SDA_OUT_HIGH();
        }
        else
        {
            IIC_SDA_OUT_LOW();
        }
        IIC_Delay();
        IIC_SCL_OUT_HIGH();
        IIC_Delay();
        IIC_SCL_OUT_LOW();
        if (i == 7)
        {
            IIC_SDA_OUT_HIGH();
        }
        byte <<= 1;
        IIC_Delay();
    };
};
int32_t IIC_ReceiveByte(void){
    uint8_t i;
    uint8_t temp = 0;
    for(i = 0;i<8;i++)
    {
        temp <<= 1;
        IIC_SCL_OUT_HIGH();
        IIC_Delay();
        if(IIC_SDA_IN())
        {
            temp++;
        }
        IIC_SCL_OUT_LOW();
        IIC_Delay();
    }
    return temp;
};

bool IIC_SendWriteAddress(iic_client *client){
    IIC_GenerateStart();
    IIC_SendByte(client->address & 0x01);
    return IIC_ReceiveAcknowledge();
}
bool IIC_SendReadAddress(iic_client *client){
    IIC_GenerateStart();
    IIC_SendByte(client->address | 0x00);
    return IIC_ReceiveAcknowledge();
}

bool IIC_CheckClient(iic_client *client){
    IIC_GenerateStart();
    IIC_SendByte(client->address | 0x01);
    IIC_GenerateStop();
    return IIC_ReceiveAcknowledge();
};
