#include "i2c_bus.h"
#include "esp_log.h"

#define TAG "I2C_BUS"

static SemaphoreHandle_t s_i2c_mutex = NULL;

void i2c_bus_init(void)
{
    if (!s_i2c_mutex) {
        s_i2c_mutex = xSemaphoreCreateMutex();
        ESP_LOGI(TAG, "I2C mutex created");
    }
}

esp_err_t i2c_bus_write(i2c_port_t port, uint8_t addr,
                         const uint8_t *data, size_t len,
                         TickType_t timeout)
{
    if (!s_i2c_mutex) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_i2c_mutex, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = i2c_master_write_to_device(
        port, addr, data, len, timeout
    );

    xSemaphoreGive(s_i2c_mutex);
    return err;
}

esp_err_t i2c_bus_read(i2c_port_t port, uint8_t addr,
                        uint8_t *data, size_t len,
                        TickType_t timeout)
{
    if (!s_i2c_mutex) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_i2c_mutex, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = i2c_master_read_from_device(
        port, addr, data, len, timeout
    );

    xSemaphoreGive(s_i2c_mutex);
    return err;
}
