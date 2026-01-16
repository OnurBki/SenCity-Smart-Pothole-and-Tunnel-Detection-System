#ifndef STORAGE_DRIVER_H
#define STORAGE_DRIVER_H

#include "esp_err.h"
#include <stdint.h>

esp_err_t storage_init(void);
void storage_write_log(int shock, uint32_t light, uint32_t time_ms);
void storage_print_all(void); // Dumps data to terminal

#endif