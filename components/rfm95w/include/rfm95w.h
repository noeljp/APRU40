#pragma once
#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_device_handle_t spi_handle;
    gpio_num_t rst_pin;
    gpio_num_t cs_pin;
    uint32_t frequency;
    uint8_t tx_power;
    uint8_t spreading_factor;
    uint32_t bandwidth;
} rfm95w_t;

/* Registres RFM95W (SX1276) */
#define RFM95W_REG_FIFO                 0x00
#define RFM95W_REG_OP_MODE              0x01
#define RFM95W_REG_FRF_MSB              0x06
#define RFM95W_REG_FRF_MID              0x07
#define RFM95W_REG_FRF_LSB              0x08
#define RFM95W_REG_PA_CONFIG            0x09
#define RFM95W_REG_LNA                  0x0C
#define RFM95W_REG_FIFO_ADDR_PTR        0x0D
#define RFM95W_REG_FIFO_TX_BASE_ADDR    0x0E
#define RFM95W_REG_FIFO_RX_BASE_ADDR    0x0F
#define RFM95W_REG_FIFO_RX_CURRENT_ADDR 0x10
#define RFM95W_REG_IRQ_FLAGS            0x12
#define RFM95W_REG_RX_NB_BYTES          0x13
#define RFM95W_REG_PKT_RSSI_VALUE       0x1A
#define RFM95W_REG_PKT_SNR_VALUE        0x1B
#define RFM95W_REG_MODEM_CONFIG_1       0x1D
#define RFM95W_REG_MODEM_CONFIG_2       0x1E
#define RFM95W_REG_PREAMBLE_MSB         0x20
#define RFM95W_REG_PREAMBLE_LSB         0x21
#define RFM95W_REG_PAYLOAD_LENGTH       0x22
#define RFM95W_REG_MODEM_CONFIG_3       0x26
#define RFM95W_REG_RSSI_WIDEBAND        0x2C
#define RFM95W_REG_DETECTION_OPTIMIZE   0x31
#define RFM95W_REG_DETECTION_THRESHOLD  0x37
#define RFM95W_REG_SYNC_WORD            0x39
#define RFM95W_REG_DIO_MAPPING_1        0x40
#define RFM95W_REG_VERSION              0x42
#define RFM95W_REG_PA_DAC               0x4D

/* Modes */
#define RFM95W_MODE_LONG_RANGE_MODE     0x80
#define RFM95W_MODE_SLEEP               0x00
#define RFM95W_MODE_STDBY               0x01
#define RFM95W_MODE_TX                  0x03
#define RFM95W_MODE_RX_CONTINUOUS       0x05
#define RFM95W_MODE_RX_SINGLE           0x06

/* IRQ Flags */
#define RFM95W_IRQ_TX_DONE              0x08
#define RFM95W_IRQ_RX_DONE              0x40
#define RFM95W_IRQ_CRC_ERROR            0x20

/* Configuration typique */
#define RFM95W_FREQ_868_MHZ             868000000
#define RFM95W_BANDWIDTH_125KHZ         125000
#define RFM95W_SPREADING_FACTOR_7       7
#define RFM95W_TX_POWER_17DBM           17

/* Init / Config */
esp_err_t rfm95w_init(rfm95w_t *dev, gpio_num_t cs, gpio_num_t rst);
esp_err_t rfm95w_reset(rfm95w_t *dev);
esp_err_t rfm95w_set_frequency(rfm95w_t *dev, uint32_t freq_hz);
esp_err_t rfm95w_set_tx_power(rfm95w_t *dev, uint8_t power);
esp_err_t rfm95w_set_spreading_factor(rfm95w_t *dev, uint8_t sf);
esp_err_t rfm95w_set_bandwidth(rfm95w_t *dev, uint32_t bw);

/* Accès registres */
esp_err_t rfm95w_write_reg(rfm95w_t *dev, uint8_t reg, uint8_t data);
esp_err_t rfm95w_read_reg(rfm95w_t *dev, uint8_t reg, uint8_t *data);

/* Mode */
esp_err_t rfm95w_set_mode(rfm95w_t *dev, uint8_t mode);
esp_err_t rfm95w_sleep(rfm95w_t *dev);
esp_err_t rfm95w_standby(rfm95w_t *dev);

/* TX/RX */
esp_err_t rfm95w_send_packet(rfm95w_t *dev, const uint8_t *data, uint8_t len);
esp_err_t rfm95w_receive_packet(rfm95w_t *dev, uint8_t *data, uint8_t *len, int16_t *rssi, int8_t *snr);
bool      rfm95w_is_packet_available(rfm95w_t *dev);

#ifdef __cplusplus
}
#endif
