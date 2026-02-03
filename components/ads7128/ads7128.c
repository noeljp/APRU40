#include "ads7128.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "i2c_bus.h"

#define TAG "ADS7128"
#define ADS7128_I2C_TIMEOUT_MS 200

/* ---------- fonctions logicielles (TOUJOURS présentes) ---------- */

void ads7128_set_vmax(ads7128_t *dev, float vmax)
{
    if (dev) dev->vmax_input = vmax;
}

float ads7128_get_vmax(const ads7128_t *dev)
{
    return dev ? dev->vmax_input : 0.0f;
}

float ads7128_raw12_to_volts(const ads7128_t *dev, uint16_t raw12)
{
    if (!dev) return 0.0f;
    return (dev->vmax_input * (float)raw12) / 4095.0f;
}


#ifdef SIMULATION_MODE

esp_err_t ads7128_init(ads7128_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->port = port;
    dev->addr = addr;
    dev->adc_ch = 0;

    ESP_LOGI(TAG, "[SIM] init addr=0x%02X", addr);
    return ESP_OK;
}

esp_err_t ads7128_set_adc_channel(ads7128_t *dev, uint8_t ch)
{
    if (!dev || ch > 7) return ESP_ERR_INVALID_ARG;
    dev->adc_ch = ch;
    return ESP_OK;
}

esp_err_t ads7128_read_adc_raw12(ads7128_t *dev, uint16_t *raw12)
{
    if (!dev || !raw12) return ESP_ERR_INVALID_ARG;

    *raw12 = 1000 + (rand() % 2000);
    ESP_LOGI(TAG, "[SIM] ADS7128 addr=0x%02X ch=%d raw=%u", dev->addr, dev->adc_ch, *raw12);
    return ESP_OK;
}


#else   // ===== REAL MODE =====

esp_err_t ads7128_init(ads7128_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->port = port;
    dev->addr = addr;
    dev->adc_ch = 0;

    ESP_LOGI(TAG, "REAL init addr=0x%02X", addr);
    return ESP_OK;
}

esp_err_t ads7128_set_adc_channel(ads7128_t *dev, uint8_t ch)
{
    if (!dev || ch > 7) return ESP_ERR_INVALID_ARG;
    dev->adc_ch = ch;
    return ads7128_write_reg(dev, ADS7128_CHANNEL_SEL_REGISTER, ch & 0x0F);
}

esp_err_t ads7128_read_adc_raw12(ads7128_t *dev, uint16_t *raw12)
{
    if (!dev || !raw12) return ESP_ERR_INVALID_ARG;

    uint8_t buf[2];
    esp_err_t err = i2c_read_bytes(dev->port, dev->addr, buf, 2);
    if (err != ESP_OK) return err;

    uint16_t raw16 = (buf[0] << 8) | buf[1];
    *raw12 = (raw16 >> 4) & 0x0FFF;
    return ESP_OK;
}

#endif

