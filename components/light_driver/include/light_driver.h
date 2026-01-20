#ifndef LIGHT_DRIVER_H
#define LIGHT_DRIVER_H

#include <stdint.h>

// GPIO 32 corresponds to ADC Unit 1, Channel 4 on ESP32
#define LDR_ADC_CHANNEL ADC_CHANNEL_4 

void light_init(void);
uint32_t light_get_level(void); // Returns 0 (Bright) - 4095 (Dark)

#endif