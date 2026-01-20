#include "mpu6050.h"
#include "esp_log.h"

static const char *TAG = "IMU_DRIVER";

// Keep these handles static so they are accessible only in this file
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

esp_err_t mpu6050_init(void)
{
    // Configure the I2C BUS
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    // Create the Bus
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    // Configure the MPU6050
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDR,
        .scl_speed_hz = 100000,
    };

    // Add Device to Bus
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
    
    // Register 0x1A (CONFIG)
    // Value 0x02 = Bandwidth ~94Hz (Delay ~3.0ms)
    // Physically ignores high-frequency low-energy events (vibrations)
    uint8_t dlpf_config[] = {MPU6050_CONFIG_REG, 0x02};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, dlpf_config, sizeof(dlpf_config), -1));

    ESP_LOGI(TAG, "MPU6050 Initialized with DLPF @ 94Hz");
    return ESP_OK;
}

uint8_t mpu6050_read_who_am_i(void)
{
    uint8_t reg_addr = WHO_AM_I_REG;
    uint8_t data = 0;

    // The New API: "Transmit then Receive" (Write Register Addr -> Read Data)
    // Handles the Start/RepeatedStart/Stop bits automatically.
    esp_err_t err = i2c_master_transmit_receive(
        dev_handle, 
        &reg_addr, 
        1,         // Size of write (1 byte register addr)
        &data, 
        1,         // Size of read (1 byte data)
        -1         // Timeout (-1 = wait forever)
    );

    if (err == ESP_OK) {
        return data;
    } else {
        ESP_LOGE(TAG, "I2C Read Failed");
        return 0xFF;
    }
}

esp_err_t mpu6050_wake_up(void)
{
    // The MPU6050 sleeps by default. Write 0 to PWR_MGMT_1 to wake it up.
    uint8_t data[] = {PWR_MGMT_1_REG, 0x00};
    
    return i2c_master_transmit(dev_handle, data, sizeof(data), -1);
}

esp_err_t mpu6050_read_accel(mpu6050_accel_t *out_data)
{
    uint8_t reg_addr = ACCEL_XOUT_H;
    uint8_t raw_data[6]; // X_H, X_L, Y_H, Y_L, Z_H, Z_L

    // Read 6 bytes starting from ACCEL_XOUT_H
    esp_err_t err = i2c_master_transmit_receive(
        dev_handle, 
        &reg_addr, 1, 
        raw_data, 6, 
        -1
    );

    if (err == ESP_OK) {
        // Combine High and Low bytes (Big Endian)
        out_data->x = (int16_t)((raw_data[0] << 8) | raw_data[1]);
        out_data->y = (int16_t)((raw_data[2] << 8) | raw_data[3]);
        out_data->z = (int16_t)((raw_data[4] << 8) | raw_data[5]);
    }
    return err;
}
