#include "rfm95w.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include "spi_bus.h"

#define TAG "RFM95W"
#define RFM95W_SPI_TIMEOUT_MS 100

#ifndef SIMULATION_MODE
/* Helpers SPI (utilisés uniquement en mode REAL) */
static esp_err_t rfm95w_spi_write(rfm95w_t *dev, uint8_t reg, uint8_t data)
{
    uint8_t tx[2] = {reg | 0x80, data};  // MSB = 1 pour écriture
    return spi_bus_transmit(dev->spi_handle, tx, NULL, 2, pdMS_TO_TICKS(RFM95W_SPI_TIMEOUT_MS));
}

static esp_err_t rfm95w_spi_read(rfm95w_t *dev, uint8_t reg, uint8_t *data)
{
    uint8_t tx[2] = {reg & 0x7F, 0x00};  // MSB = 0 pour lecture
    uint8_t rx[2] = {0};
    
    esp_err_t err = spi_bus_transmit(dev->spi_handle, tx, rx, 2, pdMS_TO_TICKS(RFM95W_SPI_TIMEOUT_MS));
    if (err == ESP_OK) {
        *data = rx[1];
    }
    return err;
}
#endif


#ifdef SIMULATION_MODE

esp_err_t rfm95w_init(rfm95w_t *dev, gpio_num_t cs, gpio_num_t rst)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    dev->rst_pin = rst;
    dev->cs_pin = cs;
    dev->frequency = RFM95W_FREQ_868_MHZ;
    dev->tx_power = RFM95W_TX_POWER_17DBM;
    dev->spreading_factor = RFM95W_SPREADING_FACTOR_7;
    dev->bandwidth = RFM95W_BANDWIDTH_125KHZ;
    
    ESP_LOGI(TAG, "[SIM] RFM95W init CS=%d RST=%d", cs, rst);
    return ESP_OK;
}

esp_err_t rfm95w_reset(rfm95w_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "[SIM] RFM95W reset");
    return ESP_OK;
}

esp_err_t rfm95w_write_reg(rfm95w_t *dev, uint8_t reg, uint8_t data)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

esp_err_t rfm95w_read_reg(rfm95w_t *dev, uint8_t reg, uint8_t *data)
{
    if (!dev || !data) return ESP_ERR_INVALID_ARG;
    *data = (reg == RFM95W_REG_VERSION) ? 0x12 : 0x00;
    return ESP_OK;
}

esp_err_t rfm95w_set_mode(rfm95w_t *dev, uint8_t mode)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

esp_err_t rfm95w_sleep(rfm95w_t *dev)
{
    return rfm95w_set_mode(dev, RFM95W_MODE_SLEEP);
}

esp_err_t rfm95w_standby(rfm95w_t *dev)
{
    return rfm95w_set_mode(dev, RFM95W_MODE_STDBY);
}

esp_err_t rfm95w_set_frequency(rfm95w_t *dev, uint32_t freq_hz)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->frequency = freq_hz;
    ESP_LOGI(TAG, "[SIM] Set frequency: %lu Hz", (unsigned long)freq_hz);
    return ESP_OK;
}

esp_err_t rfm95w_set_tx_power(rfm95w_t *dev, uint8_t power)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->tx_power = power;
    return ESP_OK;
}

esp_err_t rfm95w_set_spreading_factor(rfm95w_t *dev, uint8_t sf)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->spreading_factor = sf;
    return ESP_OK;
}

esp_err_t rfm95w_set_bandwidth(rfm95w_t *dev, uint32_t bw)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    dev->bandwidth = bw;
    return ESP_OK;
}

esp_err_t rfm95w_send_packet(rfm95w_t *dev, const uint8_t *data, uint8_t len)
{
    if (!dev || !data) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "[SIM] Send packet len=%d", len);
    return ESP_OK;
}

bool rfm95w_is_packet_available(rfm95w_t *dev)
{
    static uint32_t counter = 0;
    counter++;
    return (counter % 10 == 0); // Simule réception tous les 10 appels
}

esp_err_t rfm95w_receive_packet(rfm95w_t *dev, uint8_t *data, uint8_t *len, int16_t *rssi, int8_t *snr)
{
    if (!dev || !data || !len) return ESP_ERR_INVALID_ARG;
    
    // Simulation
    const char *msg = "SIM_DATA";
    *len = strlen(msg);
    memcpy(data, msg, *len);
    
    if (rssi) *rssi = -80;
    if (snr) *snr = 8;
    
    ESP_LOGI(TAG, "[SIM] Received packet len=%d RSSI=%d SNR=%d", *len, -80, 8);
    return ESP_OK;
}


#else   // ===== REAL MODE =====

esp_err_t rfm95w_init(rfm95w_t *dev, gpio_num_t cs, gpio_num_t rst)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    dev->rst_pin = rst;
    dev->cs_pin = cs;
    
    // Config GPIO RST
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << rst),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Config device SPI (le bus est déjà initialisé par spi_bus_init)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,  // 10 MHz
        .mode = 0,                            // SPI mode 0
        .spics_io_num = cs,
        .queue_size = 7,
        .flags = 0,
    };
    
    esp_err_t err = spi_bus_add_device(SPI_BUS_HOST, &devcfg, &dev->spi_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Reset hardware
    err = rfm95w_reset(dev);
    if (err != ESP_OK) return err;
    
    // Vérifier version
    uint8_t version;
    err = rfm95w_read_reg(dev, RFM95W_REG_VERSION, &version);
    if (err != ESP_OK || version != 0x12) {
        ESP_LOGE(TAG, "Version check failed: 0x%02X (expected 0x12)", version);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "REAL RFM95W init OK, version=0x%02X", version);
    
    // Config LoRa mode
    err = rfm95w_set_mode(dev, RFM95W_MODE_SLEEP);
    if (err != ESP_OK) return err;
    
    err = rfm95w_write_reg(dev, RFM95W_REG_OP_MODE, RFM95W_MODE_LONG_RANGE_MODE | RFM95W_MODE_SLEEP);
    if (err != ESP_OK) return err;
    
    // Config par défaut
    dev->frequency = RFM95W_FREQ_868_MHZ;
    dev->tx_power = RFM95W_TX_POWER_17DBM;
    dev->spreading_factor = RFM95W_SPREADING_FACTOR_7;
    dev->bandwidth = RFM95W_BANDWIDTH_125KHZ;
    
    rfm95w_set_frequency(dev, dev->frequency);
    rfm95w_set_tx_power(dev, dev->tx_power);
    rfm95w_set_spreading_factor(dev, dev->spreading_factor);
    rfm95w_set_bandwidth(dev, dev->bandwidth);
    
    return rfm95w_standby(dev);
}

esp_err_t rfm95w_reset(rfm95w_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    gpio_set_level(dev->rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(dev->rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    return ESP_OK;
}

esp_err_t rfm95w_write_reg(rfm95w_t *dev, uint8_t reg, uint8_t data)
{
    return rfm95w_spi_write(dev, reg, data);
}

esp_err_t rfm95w_read_reg(rfm95w_t *dev, uint8_t reg, uint8_t *data)
{
    return rfm95w_spi_read(dev, reg, data);
}

esp_err_t rfm95w_set_mode(rfm95w_t *dev, uint8_t mode)
{
    return rfm95w_write_reg(dev, RFM95W_REG_OP_MODE, RFM95W_MODE_LONG_RANGE_MODE | mode);
}

esp_err_t rfm95w_sleep(rfm95w_t *dev)
{
    return rfm95w_set_mode(dev, RFM95W_MODE_SLEEP);
}

esp_err_t rfm95w_standby(rfm95w_t *dev)
{
    return rfm95w_set_mode(dev, RFM95W_MODE_STDBY);
}

esp_err_t rfm95w_set_frequency(rfm95w_t *dev, uint32_t freq_hz)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000;
    
    esp_err_t err = rfm95w_write_reg(dev, RFM95W_REG_FRF_MSB, (uint8_t)(frf >> 16));
    if (err != ESP_OK) return err;
    
    err = rfm95w_write_reg(dev, RFM95W_REG_FRF_MID, (uint8_t)(frf >> 8));
    if (err != ESP_OK) return err;
    
    err = rfm95w_write_reg(dev, RFM95W_REG_FRF_LSB, (uint8_t)(frf >> 0));
    if (err == ESP_OK) {
        dev->frequency = freq_hz;
    }
    
    return err;
}

esp_err_t rfm95w_set_tx_power(rfm95w_t *dev, uint8_t power)
{
    if (!dev || power > 23) return ESP_ERR_INVALID_ARG;
    
    uint8_t pa_config = 0x80 | (power - 2);  // PA_BOOST
    esp_err_t err = rfm95w_write_reg(dev, RFM95W_REG_PA_CONFIG, pa_config);
    
    if (err == ESP_OK) {
        dev->tx_power = power;
    }
    
    return err;
}

esp_err_t rfm95w_set_spreading_factor(rfm95w_t *dev, uint8_t sf)
{
    if (!dev || sf < 6 || sf > 12) return ESP_ERR_INVALID_ARG;
    
    uint8_t config;
    esp_err_t err = rfm95w_read_reg(dev, RFM95W_REG_MODEM_CONFIG_2, &config);
    if (err != ESP_OK) return err;
    
    config = (config & 0x0F) | ((sf << 4) & 0xF0);
    err = rfm95w_write_reg(dev, RFM95W_REG_MODEM_CONFIG_2, config);
    
    if (err == ESP_OK) {
        dev->spreading_factor = sf;
    }
    
    return err;
}

esp_err_t rfm95w_set_bandwidth(rfm95w_t *dev, uint32_t bw)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    
    uint8_t bw_val = 7;  // 125 kHz par défaut
    if (bw >= 250000) bw_val = 8;
    if (bw >= 500000) bw_val = 9;
    
    uint8_t config;
    esp_err_t err = rfm95w_read_reg(dev, RFM95W_REG_MODEM_CONFIG_1, &config);
    if (err != ESP_OK) return err;
    
    config = (config & 0x0F) | (bw_val << 4);
    err = rfm95w_write_reg(dev, RFM95W_REG_MODEM_CONFIG_1, config);
    
    if (err == ESP_OK) {
        dev->bandwidth = bw;
    }
    
    return err;
}

esp_err_t rfm95w_send_packet(rfm95w_t *dev, const uint8_t *data, uint8_t len)
{
    if (!dev || !data || len > 255) return ESP_ERR_INVALID_ARG;
    
    // Standby
    esp_err_t err = rfm95w_standby(dev);
    if (err != ESP_OK) return err;
    
    // FIFO base address
    err = rfm95w_write_reg(dev, RFM95W_REG_FIFO_ADDR_PTR, 0);
    if (err != ESP_OK) return err;
    
    err = rfm95w_write_reg(dev, RFM95W_REG_FIFO_TX_BASE_ADDR, 0);
    if (err != ESP_OK) return err;
    
    // Écrire payload
    for (uint8_t i = 0; i < len; i++) {
        err = rfm95w_write_reg(dev, RFM95W_REG_FIFO, data[i]);
        if (err != ESP_OK) return err;
    }
    
    err = rfm95w_write_reg(dev, RFM95W_REG_PAYLOAD_LENGTH, len);
    if (err != ESP_OK) return err;
    
    // TX mode
    err = rfm95w_set_mode(dev, RFM95W_MODE_TX);
    if (err != ESP_OK) return err;
    
    // Attendre TX done (simplification)
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Clear IRQ
    err = rfm95w_write_reg(dev, RFM95W_REG_IRQ_FLAGS, 0xFF);
    
    return rfm95w_standby(dev);
}

bool rfm95w_is_packet_available(rfm95w_t *dev)
{
    if (!dev) return false;
    
    uint8_t irq_flags;
    if (rfm95w_read_reg(dev, RFM95W_REG_IRQ_FLAGS, &irq_flags) != ESP_OK) {
        return false;
    }
    
    return (irq_flags & RFM95W_IRQ_RX_DONE) != 0;
}

esp_err_t rfm95w_receive_packet(rfm95w_t *dev, uint8_t *data, uint8_t *len, int16_t *rssi, int8_t *snr)
{
    if (!dev || !data || !len) return ESP_ERR_INVALID_ARG;
    
    // Lire taille
    uint8_t packet_len;
    esp_err_t err = rfm95w_read_reg(dev, RFM95W_REG_RX_NB_BYTES, &packet_len);
    if (err != ESP_OK) return err;
    
    // Lire adresse FIFO
    uint8_t fifo_addr;
    err = rfm95w_read_reg(dev, RFM95W_REG_FIFO_RX_CURRENT_ADDR, &fifo_addr);
    if (err != ESP_OK) return err;
    
    err = rfm95w_write_reg(dev, RFM95W_REG_FIFO_ADDR_PTR, fifo_addr);
    if (err != ESP_OK) return err;
    
    // Lire données
    for (uint8_t i = 0; i < packet_len && i < *len; i++) {
        err = rfm95w_read_reg(dev, RFM95W_REG_FIFO, &data[i]);
        if (err != ESP_OK) return err;
    }
    *len = packet_len;
    
    // Lire RSSI et SNR
    if (rssi) {
        uint8_t rssi_val;
        rfm95w_read_reg(dev, RFM95W_REG_PKT_RSSI_VALUE, &rssi_val);
        *rssi = -157 + rssi_val;
    }
    
    if (snr) {
        uint8_t snr_val;
        rfm95w_read_reg(dev, RFM95W_REG_PKT_SNR_VALUE, &snr_val);
        *snr = (int8_t)snr_val / 4;
    }
    
    // Clear IRQ
    err = rfm95w_write_reg(dev, RFM95W_REG_IRQ_FLAGS, 0xFF);
    
    return ESP_OK;
}

#endif
