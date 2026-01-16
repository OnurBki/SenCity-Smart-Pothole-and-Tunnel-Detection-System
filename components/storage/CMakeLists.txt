idf_component_register(SRCS "storage_driver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_partition esp_common spiffs) 
                       # esp_partition is needed for internal flash storage access