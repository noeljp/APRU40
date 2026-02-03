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
    uint8_t    addr;        // 7-bit I2C address (0x40 ou 0x41)
    float      vref;        // tension de référence interne (2.048V typique)
    uint8_t    gain;        // gain courant (1, 2, 4)
    uint8_t    mux_config;  // configuration du multiplexeur
} ads1119_t;

/* Registres ADS1119 */
#define ADS1119_REG_CONFIG    0x00
#define ADS1119_REG_STATUS    0x01
#define ADS1119_REG_DATA      0x02

/* Commandes */
#define ADS1119_CMD_RESET      0x06
#define ADS1119_CMD_START_SYNC 0x08
#define ADS1119_CMD_POWERDOWN  0x02
#define ADS1119_CMD_RDATA      0x10
#define ADS1119_CMD_RREG       0x20
#define ADS1119_CMD_WREG       0x40

/* Configuration bits du registre CONFIG (0x00) */
/* Bit 7: VREF (0=internal 2.048V, 1=external REFP/REFN) */
#define ADS1119_VREF_INTERNAL 0x00
#define ADS1119_VREF_EXTERNAL 0x80

/* Bits 6-5: CM (Conversion mode) */
#define ADS1119_CM_SINGLE     0x00  // Single-shot
#define ADS1119_CM_CONTINUOUS 0x20  // Continuous

/* Bits 4-2: MUX (Input multiplexer) */
#define ADS1119_MUX_AIN0_AIN1 0x00  // Diff: AIN0 - AIN1
#define ADS1119_MUX_AIN2_AIN3 0x04  // Diff: AIN2 - AIN3
#define ADS1119_MUX_AIN1_AIN2 0x08  // Diff: AIN1 - AIN2
#define ADS1119_MUX_AIN0      0x0C  // Single: AIN0
#define ADS1119_MUX_AIN1      0x10  // Single: AIN1
#define ADS1119_MUX_AIN2      0x14  // Single: AIN2
#define ADS1119_MUX_AIN3      0x18  // Single: AIN3
#define ADS1119_MUX_AVSS      0x1C  // Single: AVSS (mid-supply)

/* Bits 1-0: GAIN */
#define ADS1119_GAIN_1        0x00  // ±2.048V (ou ±VREF)
#define ADS1119_GAIN_4        0x01  // ±0.512V (ou ±VREF/4)

/* Status register bits */
#define ADS1119_STATUS_DRDY   0x80  // Data ready (0=ready)


/* Init / paramètres */
esp_err_t ads1119_init(ads1119_t *dev, i2c_port_t port, uint8_t addr_7bit);
void      ads1119_set_vref(ads1119_t *dev, float vref);
float     ads1119_get_vref(const ads1119_t *dev);

/* Configuration */
esp_err_t ads1119_write_config(ads1119_t *dev, uint8_t config);
esp_err_t ads1119_read_config(ads1119_t *dev, uint8_t *config);

/* Commandes */
esp_err_t ads1119_reset(ads1119_t *dev);
esp_err_t ads1119_start_conversion(ads1119_t *dev);
esp_err_t ads1119_powerdown(ads1119_t *dev);

/* Configuration des canaux */
esp_err_t ads1119_set_channel(ads1119_t *dev, uint8_t ch);  // Équivalent à ads7128_set_adc_channel
esp_err_t ads1119_set_mux(ads1119_t *dev, uint8_t mux);
esp_err_t ads1119_set_gain(ads1119_t *dev, uint8_t gain);
esp_err_t ads1119_set_mode(ads1119_t *dev, uint8_t mode);

/* Lecture ADC (16 bits signé) */
esp_err_t ads1119_read_raw16(ads1119_t *dev, int16_t *raw16);
esp_err_t ads1119_read_status(ads1119_t *dev, uint8_t *status);
bool      ads1119_is_data_ready(uint8_t status);

/* Conversion */
float ads1119_raw16_to_volts(const ads1119_t *dev, int16_t raw16);

#ifdef __cplusplus
}
#endif
