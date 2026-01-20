#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "mpu6050.h"  
#include "storage_driver.h"
#include "light_driver.h"
#include "wifi_driver.h"
#include "web_server.h"

static const char *TAG = "CITY_SENSE";

// --- SYSTEM CONFIGURATION ---

// --- HARDWARE PINS ---
#define CONFIG_BUZZER_PIN           13
#define CONFIG_LED_PIN              12

// --- SHOCK SENSOR TUNING (Raw Acceleration Units) ---
// Base noise floor (Engine hum, smooth road)
#define THRESHOLD_SHOCK_NOISE       4000  
// Minor bumps (Small holes)
#define THRESHOLD_SHOCK_MINOR       9000  
// Suspension-breaking impacts
#define THRESHOLD_SHOCK_MAJOR       15000 

// --- LIGHT SENSOR TUNING (0-4095) ---
// Higher value = Darker
// Two values (Hysteresis) to prevent flickering at the tunnel entrance.
#define THRESHOLD_LDR_ENTER_TUNNEL  3000   // Must rise above this to enter
#define THRESHOLD_LDR_EXIT_TUNNEL   2000  // Must fall below this to exit

// --- FILTERING & TIMING ---
#define FILTER_ALPHA                0.9f  // Gravity filter strength (0.0 - 1.0)
#define SAMPLING_RATE_MS            20    // 50Hz Sensor Loop
#define LIGHT_CHECK_RATE_MS         500   // 2Hz Light Loop
#define DEBOUNCE_MAJOR_MS           1000  // Debounce after big hit
#define DEBOUNCE_MINOR_MS           500   // Debounce after small hit
#define BEEP_DURATION_MS            500   // Buzzer beep length (ms)

// --- GLOBAL STATE ---
volatile bool is_tunnel_active = false;

// --- TASKS ---

// --- TASK 1: LIGHT MONITOR (Core 0) ---
// Detects Tunnels vs Daylight
void light_task(void *pvParameter)
{
    light_init();

    // Setup LED Pin
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // Initial State Check
    uint32_t current_light = light_get_level();
    
    // REVERSED LOGIC: High Value = Dark (Tunnel)
    if (current_light > THRESHOLD_LDR_ENTER_TUNNEL) {
        is_tunnel_active = true;
    }
    
    ESP_LOGI("LIGHT", "Monitor Started. Initial Level: %lu (Dark=4095)", current_light);

    while (1) {
        current_light = light_get_level();

        if (is_tunnel_active) {
            // STATE: INSIDE TUNNEL (Dark)
            // Look for LOW numbers (Bright) to exit
            if (current_light < THRESHOLD_LDR_EXIT_TUNNEL) {
                is_tunnel_active = false;

                // ACTION: LIGHTS OFF
                gpio_set_level(CONFIG_LED_PIN, 0);

                ESP_LOGI("LIGHT", "<<< TUNNEL EXIT (Level: %lu)", current_light);
            }
        } 
        else {
            // STATE: OUTDOORS (Bright)
            // Look for HIGH numbers (Dark) to enter
            if (current_light > THRESHOLD_LDR_ENTER_TUNNEL) {
                is_tunnel_active = true;

                // ACTION: LIGHTS ON
                gpio_set_level(CONFIG_LED_PIN, 1);
                
                ESP_LOGW("LIGHT", ">>> TUNNEL ENTRY (Level: %lu)", current_light);
                
                // Mark event in storage (Shock=0 is marker)
                storage_write_log(0, current_light, xTaskGetTickCount() * portTICK_PERIOD_MS);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(LIGHT_CHECK_RATE_MS));
    }
}

// --- TASK 2: SENSOR FUSION (Core 1) ---
// Detects Potholes + Correlates with Tunnel State
void sensor_task(void *pvParameter)
{
    // Hardware Init
    mpu6050_init();
    mpu6050_wake_up();
    
    // Buzzer Init
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_BUZZER_PIN);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(CONFIG_BUZZER_PIN, 0);

    mpu6050_accel_t raw;
    float grav_z = 0;
    
    // Fast Stabilization
    if(mpu6050_read_accel(&raw) == ESP_OK) grav_z = raw.z;

    while (1) {
        int next_delay = SAMPLING_RATE_MS;

        if (mpu6050_read_accel(&raw) == ESP_OK) {
            
            // High Pass Filter (Isolate Shock from Gravity)
            grav_z = (FILTER_ALPHA * grav_z) + ((1.0 - FILTER_ALPHA) * raw.z);
            int shock = abs((int)(raw.z - grav_z));

            // Noise Gate
            if (shock > THRESHOLD_SHOCK_NOISE) {
                
                // --- CLASSIFICATION ---
                bool is_major = (shock > THRESHOLD_SHOCK_MAJOR);
                bool is_minor = (shock > THRESHOLD_SHOCK_MINOR);

                if (is_major || is_minor) {
                    
                    // Contextual Tagging (Sensor Fusion)
                    const char* env_status = is_tunnel_active ? "IN TUNNEL" : "OUTDOORS";
                    const char* severity   = is_major ? "CRITICAL" : "ALERT";
                    
                    // Console Log
                    if (is_major) {
                        ESP_LOGE(TAG, "%s POTHOLE (%s)! Shock: %d", severity, env_status, shock);
                    } else {
                        ESP_LOGW(TAG, "%s BUMP (%s) | Shock: %d", severity, env_status, shock);
                    }
                    
                    // Black Box Save
                    storage_write_log(shock, light_get_level(), xTaskGetTickCount() * portTICK_PERIOD_MS);
                    
                    // Audio Feedback (Beep)
                    if (is_major) {
                        gpio_set_level(CONFIG_BUZZER_PIN, 1);
                        vTaskDelay(pdMS_TO_TICKS(BEEP_DURATION_MS));
                        gpio_set_level(CONFIG_BUZZER_PIN, 0);
                        
                        // Set dynamic delay to account for beep time
                        next_delay = DEBOUNCE_MAJOR_MS - BEEP_DURATION_MS;
                    } else {
                        next_delay = DEBOUNCE_MINOR_MS;
                    }
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(next_delay));
    }
}

// --- MAIN ENTRY ---
void app_main(void)
{
    ESP_LOGI(TAG, "CitySense v1.0 Booting...");
    
    // --- 1. INITIALIZE NVS (CRITICAL FOR WIFI) ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // --- 2. MOUNT STORAGE ---
    if (storage_init() != ESP_OK) return;

    // --- 3. START WIFI & SERVER ---
    wifi_init_softap();
    start_webserver();

    // --- 4. START SENSORS ---
    xTaskCreatePinnedToCore(light_task, "light_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 4096, NULL, 5, NULL, 1);
}