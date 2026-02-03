#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"
#include "esp_err.h"

void i2c_bus_init(void);

/* Helpers thread-safe */
esp_err_t i2c_bus_write(i2c_port_t port, uint8_t addr,
                         const uint8_t *data, size_t len,
                         TickType_t timeout);

esp_err_t i2c_bus_read(i2c_port_t port, uint8_t addr,
                        uint8_t *data, size_t len,
                        TickType_t timeout);
