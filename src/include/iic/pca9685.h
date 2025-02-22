#pragma once

#include "krlibc.h"
#include "iic/iic_basic_implementation.h"

#ifdef IIC_BASIC_IMPLEMENTATION
#  define PCA9685_IMPLEMENTATION
#endif


#define __SUBADR1        0x02
#define __SUBADR2        0x03
#define __SUBADR3        0x04
#define __MODE1          0x00
#define __PRESCALE       0xFE
#define __LED0_ON_L      0x06
#define __LED0_ON_H      0x07
#define __LED0_OFF_L     0x08
#define __LED0_OFF_H     0x09
#define __ALLLED_ON_L    0xFA
#define __ALLLED_ON_H    0xFB
#define __ALLLED_OFF_L   0xFC
#define __ALLLED_OFF_H   0xFD

#ifdef PCA9685_IMPLEMENTATION

void pca9685_probe(void);
void pca9685_remove(void);
IIC_Slave init_pca9685(uint32_t );
void pca9685_write(uint8_t , uint8_t );
uint8_t pca9685_read(uint8_t );
void pca9685_setPWMFreq(uint32_t );
void pca9685_setPWM(uint8_t , uint8_t , uint8_t );
void pca9685_setServoPulse(uint8_t , uint32_t );

#endif
