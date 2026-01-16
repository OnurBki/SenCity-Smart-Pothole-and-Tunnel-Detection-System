#include "light_driver.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static adc_oneshot_unit_handle_t adc1_handle;

void light_init(void)
{
    // 1. Initialize ADC Unit 1
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 2. Configure Channel (GPIO 34)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12, // 12-bit resolution (0-4095)
        .atten = ADC_ATTEN_DB_12,    // Allow measuring voltages up to ~3.3V
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LDR_ADC_CHANNEL, &config));
}

uint32_t light_get_level(void)
{
    int raw_val;
    // Take average of 5 samples for stability
    uint32_t sum = 0;
    for(int i=0; i<5; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR_ADC_CHANNEL, &raw_val));
        sum += raw_val;
    }
    return sum / 5;
}