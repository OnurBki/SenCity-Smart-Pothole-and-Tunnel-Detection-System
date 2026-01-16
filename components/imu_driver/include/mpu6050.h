#ifndef MPU6050_H
#define MPU6050_H

#include "esp_err.h"
#include "driver/i2c_master.h"

// Hardware Config
#define I2C_MASTER_SCL_IO           22    
#define I2C_MASTER_SDA_IO           21    
#define I2C_PORT_NUM                0
#define MPU6050_ADDR                0x68

// Registers
#define WHO_AM_I_REG                0x75
#define PWR_MGMT_1_REG              0x6B
#define ACCEL_XOUT_H                0x3B  // Start of Accel Data
#define MPU6050_CONFIG_REG 0x1A

// Data Structure for efficiency
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} mpu6050_accel_t;

// API
esp_err_t mpu6050_init(void);
uint8_t mpu6050_read_who_am_i(void);
esp_err_t mpu6050_wake_up(void);
esp_err_t mpu6050_read_accel(mpu6050_accel_t *out_data);

#endif