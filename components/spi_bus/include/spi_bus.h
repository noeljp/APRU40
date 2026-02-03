#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

/* Configuration du bus SPI commun */
#define SPI_BUS_MISO    GPIO_NUM_2
#define SPI_BUS_MOSI    GPIO_NUM_15
#define SPI_BUS_CLK     GPIO_NUM_14
#define SPI_BUS_HOST    SPI2_HOST

void spi_bus_init(void);

/* Helpers thread-safe pour SPI */
esp_err_t spi_bus_transmit(spi_device_handle_t handle,
                            const uint8_t *tx_data, 
                            uint8_t *rx_data,
                            size_t len,
                            TickType_t timeout);
