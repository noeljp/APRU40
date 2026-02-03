#include "spi_bus.h"
#include "esp_log.h"

#define TAG "SPI_BUS"

static SemaphoreHandle_t s_spi_mutex = NULL;
static bool s_spi_bus_initialized = false;

void spi_bus_init(void)
{
    if (!s_spi_mutex) {
        s_spi_mutex = xSemaphoreCreateMutex();
        ESP_LOGI(TAG, "SPI mutex created");
    }
    
    if (!s_spi_bus_initialized) {
        spi_bus_config_t buscfg = {
            .miso_io_num = SPI_BUS_MISO,
            .mosi_io_num = SPI_BUS_MOSI,
            .sclk_io_num = SPI_BUS_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4096,
        };
        
        esp_err_t err = spi_bus_initialize(SPI_BUS_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (err == ESP_OK) {
            s_spi_bus_initialized = true;
            ESP_LOGI(TAG, "SPI bus initialized: MISO=%d MOSI=%d CLK=%d", 
                     SPI_BUS_MISO, SPI_BUS_MOSI, SPI_BUS_CLK);
        } else if (err == ESP_ERR_INVALID_STATE) {
            ESP_LOGI(TAG, "SPI bus already initialized");
            s_spi_bus_initialized = true;
        } else {
            ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(err));
        }
    }
}

esp_err_t spi_bus_transmit(spi_device_handle_t handle,
                            const uint8_t *tx_data, 
                            uint8_t *rx_data,
                            size_t len,
                            TickType_t timeout)
{
    if (!s_spi_mutex) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_spi_mutex, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    spi_transaction_t trans = {
        .length = len * 8,  // en bits
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    esp_err_t err = spi_device_transmit(handle, &trans);

    xSemaphoreGive(s_spi_mutex);
    return err;
}
