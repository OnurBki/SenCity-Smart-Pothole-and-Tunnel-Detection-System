idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES imu_driver light_driver storage wifi_driver web_server esp_driver_gpio driver nvs_flash)