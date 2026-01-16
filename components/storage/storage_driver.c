#include "storage_driver.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "STORAGE";

esp_err_t storage_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret == ESP_OK) {
        size_t total = 0, used = 0;
        esp_spiffs_info(conf.partition_label, &total, &used);
        ESP_LOGI(TAG, "Storage Ready. Used: %d KB", used/1024);
    }
    return ret;
}

void storage_write_log(int shock, uint32_t light, uint32_t time_ms)
{
    FILE* f = fopen("/spiffs/log.csv", "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "File Error");
        return;
    }
    // Format: Time(ms), Shock, LightLevel
    fprintf(f, "%lu,%d,%lu\n", time_ms, shock, light);
    fclose(f);
    ESP_LOGI(TAG, "Event Saved.");
}

void storage_print_all(void)
{
    ESP_LOGW(TAG, "--- DUMPING DATA ---");
    ESP_LOGW(TAG, "Time(ms), Shock, Light");
    
    FILE* f = fopen("/spiffs/log.csv", "r");
    if (f != NULL) {
        char line[64];
        while (fgets(line, sizeof(line), f) != NULL) {
            printf("%s", line); 
        }
        fclose(f);
    }
    ESP_LOGW(TAG, "--- END DUMP ---");
}