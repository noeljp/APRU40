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
    uint8_t    addr;        // 7-bit I2C address (0x49 par défaut)
    uint8_t    output_state; // État courant des sorties
    uint8_t    config;      // Configuration pins (0=output, 1=input)
} tca9537_t;

/* Registres TCA9537 */
#define TCA9537_REG_INPUT_PORT    0x00  // Input port register (read)
#define TCA9537_REG_OUTPUT_PORT   0x01  // Output port register (read/write)
#define TCA9537_REG_POLARITY      0x02  // Polarity inversion register
#define TCA9537_REG_CONFIG        0x03  // Configuration register (0=output, 1=input)

/* Pins */
#define TCA9537_PIN0  (1 << 0)
#define TCA9537_PIN1  (1 << 1)
#define TCA9537_PIN2  (1 << 2)
#define TCA9537_PIN3  (1 << 3)
#define TCA9537_ALL_PINS  0x0F

/* Init / Config */
esp_err_t tca9537_init(tca9537_t *dev, i2c_port_t port, uint8_t addr_7bit);
esp_err_t tca9537_set_pin_mode(tca9537_t *dev, uint8_t pin_mask, bool is_input);

/* Accès registres */
esp_err_t tca9537_write_reg(tca9537_t *dev, uint8_t reg, uint8_t data);
esp_err_t tca9537_read_reg(tca9537_t *dev, uint8_t reg, uint8_t *data);

/* GPIO operations (API cohérente avec autres drivers) */
esp_err_t tca9537_write_pins(tca9537_t *dev, uint8_t pin_mask, bool state);
esp_err_t tca9537_set_pin(tca9537_t *dev, uint8_t pin_mask);    // Set high
esp_err_t tca9537_clear_pin(tca9537_t *dev, uint8_t pin_mask);  // Set low
esp_err_t tca9537_toggle_pin(tca9537_t *dev, uint8_t pin_mask); // Toggle
esp_err_t tca9537_read_pins(tca9537_t *dev, uint8_t *pin_state);

#ifdef __cplusplus
}
#endif
