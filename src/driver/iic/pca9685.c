//enable all_implementation
#define ALL_IMPLEMENTATION

#include "iic/pca9685.h"

/**
 * Initialize the PCA9685 device with the given I2C address.
 *
 * @param address The I2C address of the PCA9685 device.
 */
IIC_Slave init_pca9685(uint32_t address){
    IIC_Slave pca9685 = {
        .address = address,
        .reg_address = __SUBADR1,
        .freq = Freq_400,
        .flag = IIC_Slave_Flag_Empty,
        .probe = pca9685_probe,
        .remove = pca9685_remove,
    };
    return pca9685;
}

/**
 * Probe the PCA9685 device.
 */
void pca9685_probe(void){
    //挂载兼容性
}

/**
 * Remove the PCA9685 device.
 */
void pca9685_remove(void){
    //移除兼容性
}

/**
 * Write a byte to the PCA9685 device at the given register address.
 *
 * @param reg The register address to write to.
 * @param data The byte to write.
 */
void pca9685_write(uint8_t reg, uint8_t data){
    //写入兼容性
    UNUSED(reg);
    UNUSED(data);
}

/**
 * Read a byte from the PCA9685 device at the given register address.
 *
 * @param reg The register address to read from.
 *
 * @return The byte read from the PCA9685 device.
 */
uint8_t pca9685_read(uint8_t reg){
    //读取兼容性
    UNUSED(reg);
}

/**
 * Set the PWM frequency of the PCA9685 device.
 *
 * @param freq The new frequency for the PCA9685 device, in Hz.
 */
void pca9685_setPWMFreq(uint32_t freq){
    //设置Freq兼容性
    UNUSED(freq);
}

/**
 * Set the PWM value for the PCA9685 device on the given channel.
 *
 * @param channel The channel to set the PWM value for, in the range 0 to 15.
 * @param on The value to set for the "on" duty cycle, in the range 0 to 4095.
 * @param off The value to set for the "off" duty cycle, in the range 0 to 4095.
 */
void pca9685_setPWM(uint8_t channel, uint8_t on, uint8_t off){
    //PWM兼容性
    UNUSED(channel);
    UNUSED(on);
    UNUSED(off);
}

/**
 * Set the servo pulse width for the PCA9685 device on the given channel.
 *
 * @param channel The channel to set the servo pulse width for, in the range 0 to 15.
 * @param pulse The pulse width to set for the servo, in microseconds.
 */
void pca9685_setServoPulse(uint8_t channel, uint32_t pulse){
    //Servo脉冲兼容性
    UNUSED(channel);
    UNUSED(pulse);
}
