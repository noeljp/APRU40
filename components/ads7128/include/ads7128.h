#pragma once
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_port_t port;
    uint8_t    addr;        // 7-bit I2C address (0x17 par défaut)
    float      vmax_input;  // pour conversion en volts
    uint8_t    adc_ch;      // canal courant 0..7
} ads7128_t;

/* Commandes (comme ton code Arduino) */
#define ADS7128_CMD_WRITE_REG   0x08
#define ADS7128_CMD_READ_REG    0x10

/* Registres (alignés avec ton header Arduino) */
#define ADS7128_SYSTEM_STATUS_REGISTER   0x00  // default 0x81 (selon datasheet)
#define ADS7128_DATA_CFG_REGISTER        0x02
#define ADS7128_OSR_CFG_REGISTER         0x03
#define ADS7128_PIN_CFG_REGISTER         0x05
#define ADS7128_GPIO_CFG_REGISTER        0x07
#define ADS7128_GPO_DRIVE_CFG_REGISTER   0x09
#define ADS7128_CHANNEL_SEL_REGISTER     0x11

/* DATA_CFG : Append ID / status flags (2 bits) */
#define ADS7128_APPEND_NONE      0x00  // 00b
#define ADS7128_APPEND_CH_ID     0x01  // 01b
#define ADS7128_APPEND_STATUS    0x02  // 10b
// 11b réservé

/* OSR ratios (3 bits) */
#define ADS7128_OSR_NONE     0x00  // 000b
#define ADS7128_OSR_2        0x01  // 001b
#define ADS7128_OSR_4        0x02  // 010b
#define ADS7128_OSR_8        0x03  // 011b
#define ADS7128_OSR_16       0x04  // 100b
#define ADS7128_OSR_32       0x05  // 101b
#define ADS7128_OSR_64       0x06  // 110b
#define ADS7128_OSR_128      0x07  // 111b



/* Init / paramètres */
esp_err_t ads7128_init(ads7128_t *dev, i2c_port_t port, uint8_t addr_7bit);
void      ads7128_set_vmax(ads7128_t *dev, float vmax);
float     ads7128_get_vmax(const ads7128_t *dev);

/* Accès registres */
esp_err_t ads7128_write_reg(ads7128_t *dev, uint8_t reg, uint8_t data);
esp_err_t ads7128_read_reg (ads7128_t *dev, uint8_t reg, uint8_t *out);

/* Fonctions équivalentes Arduino */
esp_err_t ads7128_configure_pins      (ads7128_t *dev, uint8_t analog_gpio_mask); // 0=analog, 1=gpio
esp_err_t ads7128_read_pin_config     (ads7128_t *dev, uint8_t *out);

esp_err_t ads7128_configure_gpio_dir  (ads7128_t *dev, uint8_t gpio_dir_mask);   // 0=input, 1=output
esp_err_t ads7128_read_gpio_config    (ads7128_t *dev, uint8_t *out);

esp_err_t ads7128_configure_gpo_drive (ads7128_t *dev, uint8_t gpo_drive_mask);  // 0=open-drain, 1=push-pull
esp_err_t ads7128_read_gpo_drive_config(ads7128_t *dev, uint8_t *out);

esp_err_t ads7128_set_adc_channel     (ads7128_t *dev, uint8_t ch); // 0..7

esp_err_t ads7128_set_append_id       (ads7128_t *dev, uint8_t type); // ADS7128_APPEND_*
esp_err_t ads7128_set_oversampling    (ads7128_t *dev, uint8_t osr);   // ADS7128_OSR_*
esp_err_t ads7128_get_oversampling    (ads7128_t *dev, uint8_t *out);

esp_err_t ads7128_get_system_status   (ads7128_t *dev, uint8_t *out);

/* Lecture ADC (2 bytes -> 12 bits) */
esp_err_t ads7128_read_adc_raw12      (ads7128_t *dev, uint16_t *raw12);
esp_err_t ads7128_read_adc_raw12_with_id(ads7128_t *dev, uint16_t *raw12, uint8_t *id_nibble);

/* Conversion */
float ads7128_raw12_to_volts(const ads7128_t *dev, uint16_t raw12);


#ifdef __cplusplus
}
#endif
