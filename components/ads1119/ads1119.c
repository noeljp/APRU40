#include "ads1119.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "i2c_bus.h"

#define TAG "ADS1119"
#define ADS1119_I2C_TIMEOUT_MS 200

/* ---------- fonctions logicielles (TOUJOURS présentes) ---------- */

void ads1119_set_vref(ads1119_t *dev, float vref)
{
    if (dev) dev->vref = vref;
}

float ads1119_get_vref(const ads1119_t *dev)
{
    return dev ? dev->vref : 2.048f;
}

float ads1119_raw16_to_volts(const ads1119_t *dev, int16_t raw16)
{
    if (!dev) return 0.0f;
    
    // Full scale = ±VREF/gain
    float full_scale = dev->vref;
    if (dev->gain == ADS1119_GAIN_4) {
        full_scale = dev->vref / 4.0f;
    }
    
    // ADS1119 est 16 bits signé
    return (full_scale * (float)raw16) / 32768.0f;
}

bool ads1119_is_data_ready(uint8_t status)
{
    return (status & ADS1119_STATUS_DRDY) == 0; // DRDY=0 means ready
}


#ifdef SIMULATION_MODE

esp_err_t ads1119_init(ads1119_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->port = port;
    dev->addr = addr;
    dev->vref = 2.048f;
    dev->gain = ADS1119_GAIN_1;
    dev->mux_config = ADS1119_MUX_AIN0_AIN1;

    ESP_LOGI(TAG, "[SIM] init addr=0x%02X", addr);
    return ESP_OK;
}

esp_err_t ads1119_reset(ads1119_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "[SIM] reset");
    return ESP_OK;
}

esp_err_t ads1119_start_conversion(ads1119_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    // Silencieux pour cohérence avec ADS7128
    return ESP_OK;
}

esp_err_t ads1119_powerdown(ads1119_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "[SIM] powerdown");
    return ESP_OK;
}

esp_err_t ads1119_write_config(ads1119_t *dev, uint8_t config)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->gain = config & 0x03;
    dev->mux_config = config & 0x1C;
    // Log silencieux en mode SIM pour cohérence avec ADS7128
    return ESP_OK;
}

esp_err_t ads1119_read_config(ads1119_t *dev, uint8_t *config)
{
    if (!dev || !config) return ESP_ERR_INVALID_ARG;
    *config = dev->mux_config | dev->gain;
    return ESP_OK;
}

esp_err_t ads1119_set_channel(ads1119_t *dev, uint8_t ch)
{
    if (!dev || ch > 3) return ESP_ERR_INVALID_ARG;
    
    // Mapping canal 0-3 vers MUX single-ended
    uint8_t mux_map[4] = {
        ADS1119_MUX_AIN0,
        ADS1119_MUX_AIN1,
        ADS1119_MUX_AIN2,
        ADS1119_MUX_AIN3
    };
    
    dev->mux_config = mux_map[ch];
    return ESP_OK;
}

esp_err_t ads1119_set_mux(ads1119_t *dev, uint8_t mux)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->mux_config = mux & 0x1C;
    return ESP_OK;
}

esp_err_t ads1119_set_gain(ads1119_t *dev, uint8_t gain)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->gain = gain & 0x03;
    return ESP_OK;
}

esp_err_t ads1119_set_mode(ads1119_t *dev, uint8_t mode)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

esp_err_t ads1119_read_raw16(ads1119_t *dev, int16_t *raw16)
{
    if (!dev || !raw16) return ESP_ERR_INVALID_ARG;

    // Simulation: valeur aléatoire entre -1000 et +1000
    *raw16 = (rand() % 2000) - 1000;
    ESP_LOGI(TAG, "[SIM] ADS1119 addr=0x%02X raw=%d", dev->addr, *raw16);
    return ESP_OK;
}

esp_err_t ads1119_read_status(ads1119_t *dev, uint8_t *status)
{
    if (!dev || !status) return ESP_ERR_INVALID_ARG;
    *status = 0x00; // DRDY=0 (ready)
    return ESP_OK;
}


#else   // ===== REAL MODE =====

esp_err_t ads1119_init(ads1119_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->port = port;
    dev->addr = addr;
    dev->vref = 2.048f;
    dev->gain = ADS1119_GAIN_1;
    dev->mux_config = ADS1119_MUX_AIN0_AIN1;

    ESP_LOGI(TAG, "REAL init addr=0x%02X", addr);
    
    // Reset pour configuration initiale
    return ads1119_reset(dev);
}

esp_err_t ads1119_reset(ads1119_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t cmd = ADS1119_CMD_RESET;
    return i2c_bus_write(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
}

esp_err_t ads1119_start_conversion(ads1119_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t cmd = ADS1119_CMD_START_SYNC;
    return i2c_bus_write(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
}

esp_err_t ads1119_powerdown(ads1119_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t cmd = ADS1119_CMD_POWERDOWN;
    return i2c_bus_write(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
}

esp_err_t ads1119_write_config(ads1119_t *dev, uint8_t config)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t buf[2];
    buf[0] = ADS1119_CMD_WREG | (ADS1119_REG_CONFIG << 2);
    buf[1] = config;
    
    esp_err_t err = i2c_bus_write(dev->port, dev->addr, buf, 2, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
    if (err == ESP_OK) {
        dev->gain = config & 0x03;
        dev->mux_config = config & 0x1C;
    }
    return err;
}

esp_err_t ads1119_read_config(ads1119_t *dev, uint8_t *config)
{
    if (!dev || !config) return ESP_ERR_INVALID_ARG;
    
    uint8_t cmd = ADS1119_CMD_RREG | (ADS1119_REG_CONFIG << 2);
    esp_err_t err = i2c_bus_write(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
    if (err != ESP_OK) return err;
    
    return i2c_bus_read(dev->port, dev->addr, config, 1, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
}

esp_err_t ads1119_set_channel(ads1119_t *dev, uint8_t ch)
{
    if (!dev || ch > 3) return ESP_ERR_INVALID_ARG;
    
    // Mapping canal 0-3 vers MUX single-ended
    uint8_t mux_map[4] = {
        ADS1119_MUX_AIN0,
        ADS1119_MUX_AIN1,
        ADS1119_MUX_AIN2,
        ADS1119_MUX_AIN3
    };
    
    return ads1119_set_mux(dev, mux_map[ch]);
}

esp_err_t ads1119_set_mux(ads1119_t *dev, uint8_t mux)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t config;
    esp_err_t err = ads1119_read_config(dev, &config);
    if (err != ESP_OK) return err;
    
    config = (config & ~0x1C) | (mux & 0x1C);
    return ads1119_write_config(dev, config);
}

esp_err_t ads1119_set_gain(ads1119_t *dev, uint8_t gain)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t config;
    esp_err_t err = ads1119_read_config(dev, &config);
    if (err != ESP_OK) return err;
    
    config = (config & ~0x03) | (gain & 0x03);
    return ads1119_write_config(dev, config);
}

esp_err_t ads1119_set_mode(ads1119_t *dev, uint8_t mode)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t config;
    esp_err_t err = ads1119_read_config(dev, &config);
    if (err != ESP_OK) return err;
    
    config = (config & ~0x60) | (mode & 0x60);
    return ads1119_write_config(dev, config);
}

esp_err_t ads1119_read_raw16(ads1119_t *dev, int16_t *raw16)
{
    if (!dev || !raw16) return ESP_ERR_INVALID_ARG;
    
    uint8_t cmd = ADS1119_CMD_RDATA;
    esp_err_t err = i2c_bus_write(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
    if (err != ESP_OK) return err;
    
    uint8_t buf[2];
    err = i2c_bus_read(dev->port, dev->addr, buf, 2, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
    if (err != ESP_OK) return err;
    
    *raw16 = (int16_t)((buf[0] << 8) | buf[1]);
    return ESP_OK;
}

esp_err_t ads1119_read_status(ads1119_t *dev, uint8_t *status)
{
    if (!dev || !status) return ESP_ERR_INVALID_ARG;
    
    uint8_t cmd = ADS1119_CMD_RREG | (ADS1119_REG_STATUS << 2);
    esp_err_t err = i2c_bus_write(dev->port, dev->addr, &cmd, 1, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
    if (err != ESP_OK) return err;
    
    return i2c_bus_read(dev->port, dev->addr, status, 1, pdMS_TO_TICKS(ADS1119_I2C_TIMEOUT_MS));
}

#endif
