#include "tca9537.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "i2c_bus.h"

#define TAG "TCA9537"
#define TCA9537_I2C_TIMEOUT_MS 200


#ifdef SIMULATION_MODE

esp_err_t tca9537_init(tca9537_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->port = port;
    dev->addr = addr;
    dev->output_state = 0x00;
    dev->config = 0x0F; // Toutes les pins en input par défaut

    ESP_LOGI(TAG, "[SIM] init addr=0x%02X", addr);
    return ESP_OK;
}

esp_err_t tca9537_write_reg(tca9537_t *dev, uint8_t reg, uint8_t data)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    if (reg == TCA9537_REG_OUTPUT_PORT) {
        dev->output_state = data;
    } else if (reg == TCA9537_REG_CONFIG) {
        dev->config = data;
    }
    
    return ESP_OK;
}

esp_err_t tca9537_read_reg(tca9537_t *dev, uint8_t reg, uint8_t *data)
{
    if (!dev || !data) return ESP_ERR_INVALID_ARG;
    
    if (reg == TCA9537_REG_INPUT_PORT) {
        *data = 0x05; // Simulation: pins 0 et 2 à 1
    } else if (reg == TCA9537_REG_OUTPUT_PORT) {
        *data = dev->output_state;
    } else if (reg == TCA9537_REG_CONFIG) {
        *data = dev->config;
    } else {
        *data = 0x00;
    }
    
    return ESP_OK;
}

esp_err_t tca9537_set_pin_mode(tca9537_t *dev, uint8_t pin_mask, bool is_input)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    if (is_input) {
        dev->config |= (pin_mask & 0x0F);
    } else {
        dev->config &= ~(pin_mask & 0x0F);
    }
    
    return ESP_OK;
}

esp_err_t tca9537_write_pins(tca9537_t *dev, uint8_t pin_mask, bool state)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    if (state) {
        dev->output_state |= (pin_mask & 0x0F);
    } else {
        dev->output_state &= ~(pin_mask & 0x0F);
    }
    
    ESP_LOGI(TAG, "[SIM] TCA9537 addr=0x%02X write pins=0x%02X state=%d", 
             dev->addr, pin_mask, state);
    return ESP_OK;
}

esp_err_t tca9537_set_pin(tca9537_t *dev, uint8_t pin_mask)
{
    return tca9537_write_pins(dev, pin_mask, true);
}

esp_err_t tca9537_clear_pin(tca9537_t *dev, uint8_t pin_mask)
{
    return tca9537_write_pins(dev, pin_mask, false);
}

esp_err_t tca9537_toggle_pin(tca9537_t *dev, uint8_t pin_mask)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    dev->output_state ^= (pin_mask & 0x0F);
    ESP_LOGI(TAG, "[SIM] TCA9537 addr=0x%02X toggle pins=0x%02X", dev->addr, pin_mask);
    return ESP_OK;
}

esp_err_t tca9537_read_pins(tca9537_t *dev, uint8_t *pin_state)
{
    if (!dev || !pin_state) return ESP_ERR_INVALID_ARG;
    
    *pin_state = 0x05; // Simulation: pins 0 et 2 à 1
    return ESP_OK;
}


#else   // ===== REAL MODE =====

esp_err_t tca9537_init(tca9537_t *dev, i2c_port_t port, uint8_t addr)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->port = port;
    dev->addr = addr;
    dev->output_state = 0x00;
    dev->config = 0x0F; // Toutes les pins en input par défaut

    ESP_LOGI(TAG, "REAL init addr=0x%02X", addr);
    
    // Lire la configuration actuelle
    uint8_t config;
    esp_err_t err = tca9537_read_reg(dev, TCA9537_REG_CONFIG, &config);
    if (err == ESP_OK) {
        dev->config = config;
    }
    
    return err;
}

esp_err_t tca9537_write_reg(tca9537_t *dev, uint8_t reg, uint8_t data)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = data;
    
    return i2c_bus_write(dev->port, dev->addr, buf, 2, pdMS_TO_TICKS(TCA9537_I2C_TIMEOUT_MS));
}

esp_err_t tca9537_read_reg(tca9537_t *dev, uint8_t reg, uint8_t *data)
{
    if (!dev || !data) return ESP_ERR_INVALID_ARG;
    
    esp_err_t err = i2c_bus_write(dev->port, dev->addr, &reg, 1, pdMS_TO_TICKS(TCA9537_I2C_TIMEOUT_MS));
    if (err != ESP_OK) return err;
    
    return i2c_bus_read(dev->port, dev->addr, data, 1, pdMS_TO_TICKS(TCA9537_I2C_TIMEOUT_MS));
}

esp_err_t tca9537_set_pin_mode(tca9537_t *dev, uint8_t pin_mask, bool is_input)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t config;
    esp_err_t err = tca9537_read_reg(dev, TCA9537_REG_CONFIG, &config);
    if (err != ESP_OK) return err;
    
    if (is_input) {
        config |= (pin_mask & 0x0F);
    } else {
        config &= ~(pin_mask & 0x0F);
    }
    
    err = tca9537_write_reg(dev, TCA9537_REG_CONFIG, config);
    if (err == ESP_OK) {
        dev->config = config;
    }
    
    return err;
}

esp_err_t tca9537_write_pins(tca9537_t *dev, uint8_t pin_mask, bool state)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t output;
    esp_err_t err = tca9537_read_reg(dev, TCA9537_REG_OUTPUT_PORT, &output);
    if (err != ESP_OK) return err;
    
    if (state) {
        output |= (pin_mask & 0x0F);
    } else {
        output &= ~(pin_mask & 0x0F);
    }
    
    err = tca9537_write_reg(dev, TCA9537_REG_OUTPUT_PORT, output);
    if (err == ESP_OK) {
        dev->output_state = output;
    }
    
    return err;
}

esp_err_t tca9537_set_pin(tca9537_t *dev, uint8_t pin_mask)
{
    return tca9537_write_pins(dev, pin_mask, true);
}

esp_err_t tca9537_clear_pin(tca9537_t *dev, uint8_t pin_mask)
{
    return tca9537_write_pins(dev, pin_mask, false);
}

esp_err_t tca9537_toggle_pin(tca9537_t *dev, uint8_t pin_mask)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t output;
    esp_err_t err = tca9537_read_reg(dev, TCA9537_REG_OUTPUT_PORT, &output);
    if (err != ESP_OK) return err;
    
    output ^= (pin_mask & 0x0F);
    
    err = tca9537_write_reg(dev, TCA9537_REG_OUTPUT_PORT, output);
    if (err == ESP_OK) {
        dev->output_state = output;
    }
    
    return err;
}

esp_err_t tca9537_read_pins(tca9537_t *dev, uint8_t *pin_state)
{
    if (!dev || !pin_state) return ESP_ERR_INVALID_ARG;
    
    return tca9537_read_reg(dev, TCA9537_REG_INPUT_PORT, pin_state);
}

#endif
