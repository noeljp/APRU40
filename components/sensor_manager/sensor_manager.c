/**
 * @file sensor_manager.c
 * @brief Implémentation du gestionnaire de capteurs
 */

#include "sensor_manager.h"
#include "node_config.h"
#include "conversion_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

#if ENABLE_ADS7128
#include "ads7128.h"
#endif

#if ENABLE_ADS1119_1 || ENABLE_ADS1119_2
#include "ads1119.h"
#endif

#define TAG "SENSOR_MGR"

/* ============================================================================
 * VARIABLES GLOBALES
 * ============================================================================ */

static sensor_readings_t g_readings[SENSOR_TYPE_COUNT] = {0};
static SemaphoreHandle_t g_mutex[SENSOR_TYPE_COUNT] = {NULL};
static TaskHandle_t g_tasks[SENSOR_TYPE_COUNT] = {NULL};
static sensor_manager_config_t g_config = {0};
static bool g_initialized = false;
static uint32_t g_acquisition_counts[SENSOR_TYPE_COUNT] = {0};

// Handles des drivers ADC
#if ENABLE_ADS7128
static ads7128_t g_ads7128;
#endif
#if ENABLE_ADS1119_1
static ads1119_t g_ads1119_1;
#endif
#if ENABLE_ADS1119_2
static ads1119_t g_ads1119_2;
#endif

/* ============================================================================
 * FONCTIONS DE CONVERSION
 * ============================================================================ */

/**
 * @brief Interpolation linéaire pour table de correspondance
 */
static float interpolate_lut(uint16_t raw, const uint16_t *raw_lut, const float *phys_lut, uint8_t count) {
    // Cas limites
    if (raw <= raw_lut[0]) return phys_lut[0];
    if (raw >= raw_lut[count-1]) return phys_lut[count-1];
    
    // Trouver l'intervalle
    for (uint8_t i = 0; i < count - 1; i++) {
        if (raw >= raw_lut[i] && raw <= raw_lut[i+1]) {
            float ratio = (float)(raw - raw_lut[i]) / (raw_lut[i+1] - raw_lut[i]);
            return phys_lut[i] + ratio * (phys_lut[i+1] - phys_lut[i]);
        }
    }
    return 0.0f;
}

/**
 * @brief Appliquer la conversion ADC → physique
 */
static float apply_conversion(int32_t raw, const channel_conversion_t *conv) {
    if (!conv || conv->type == CONV_NONE) {
        return (float)raw;
    }
    
    float x = (float)raw;
    
    switch (conv->type) {
        case CONV_LINEAR:
            return conv->linear.a * x + conv->linear.b;
            
        case CONV_POLYNOMIAL_2:
            return conv->poly.a * x * x + conv->poly.b * x + conv->poly.c;
            
        case CONV_POLYNOMIAL_3:
            return conv->poly.a * x * x * x + conv->poly.b * x * x + 
                   conv->poly.c * x + conv->poly.d;
                   
        case CONV_LOOKUP_TABLE:
            return interpolate_lut((uint16_t)raw, conv->lut.raw_values, 
                                   conv->lut.phys_values, conv->lut.count);
        default:
            return (float)raw;
    }
}

/* ============================================================================
 * TÂCHES D'ACQUISITION
 * ============================================================================ */

#if ENABLE_ADS7128
static void ads7128_acquisition_task(void *arg) {
    const TickType_t period = pdMS_TO_TICKS(g_config.acquisition_period_ms);
    TickType_t last_wake = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "ADS7128 acquisition task started");
    
    while (1) {
        vTaskDelayUntil(&last_wake, period);
        
        uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        xSemaphoreTake(g_mutex[SENSOR_TYPE_ADS7128], portMAX_DELAY);
        
        g_readings[SENSOR_TYPE_ADS7128].type = SENSOR_TYPE_ADS7128;
        g_readings[SENSOR_TYPE_ADS7128].num_channels = 8;
        
        // Lire tous les canaux
        for (uint8_t ch = 0; ch < 8; ch++) {
            uint16_t raw = 0;
            esp_err_t err = ads7128_set_adc_channel(&g_ads7128, ch);
            if (err == ESP_OK) {
                err = ads7128_read_adc_raw12(&g_ads7128, &raw);
            }
            
            g_readings[SENSOR_TYPE_ADS7128].channels[ch].timestamp_ms = start_time;
            g_readings[SENSOR_TYPE_ADS7128].channels[ch].raw_value = raw;
            g_readings[SENSOR_TYPE_ADS7128].channels[ch].valid = (err == ESP_OK);
            
            if (err == ESP_OK) {
                const channel_conversion_t *conv = get_ads7128_conversion(ch);
                g_readings[SENSOR_TYPE_ADS7128].channels[ch].physical_value = 
                    apply_conversion(raw, conv);
            } else {
                g_readings[SENSOR_TYPE_ADS7128].error_count++;
            }
        }
        
        g_readings[SENSOR_TYPE_ADS7128].last_update_ms = start_time;
        g_acquisition_counts[SENSOR_TYPE_ADS7128]++;
        
        xSemaphoreGive(g_mutex[SENSOR_TYPE_ADS7128]);
        
        // Callback utilisateur
        if (g_config.callback) {
            g_config.callback(SENSOR_TYPE_ADS7128, &g_readings[SENSOR_TYPE_ADS7128]);
        }
        
        // Logging optionnel
        if (g_config.enable_logging) {
            for (uint8_t ch = 0; ch < 8; ch++) {
                const channel_conversion_t *conv = get_ads7128_conversion(ch);
                if (conv && g_readings[SENSOR_TYPE_ADS7128].channels[ch].valid) {
                    ESP_LOGI(TAG, "ADS7128 CH%d [%s]: %.2f %s (raw=%ld)", 
                             ch, conv->name,
                             g_readings[SENSOR_TYPE_ADS7128].channels[ch].physical_value,
                             conv->unit,
                             g_readings[SENSOR_TYPE_ADS7128].channels[ch].raw_value);
                }
            }
        }
    }
}
#endif

#if ENABLE_ADS1119_1
static void ads1119_1_acquisition_task(void *arg) {
    const TickType_t period = pdMS_TO_TICKS(g_config.acquisition_period_ms);
    TickType_t last_wake = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "ADS1119 #1 acquisition task started");
    
    while (1) {
        vTaskDelayUntil(&last_wake, period);
        
        uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        xSemaphoreTake(g_mutex[SENSOR_TYPE_ADS1119_1], portMAX_DELAY);
        
        g_readings[SENSOR_TYPE_ADS1119_1].type = SENSOR_TYPE_ADS1119_1;
        g_readings[SENSOR_TYPE_ADS1119_1].num_channels = 4;
        
        for (uint8_t ch = 0; ch < 4; ch++) {
            int16_t raw = 0;
            esp_err_t err = ads1119_set_channel(&g_ads1119_1, ch);
            if (err == ESP_OK) {
                err = ads1119_read_raw16(&g_ads1119_1, &raw);
            }
            
            g_readings[SENSOR_TYPE_ADS1119_1].channels[ch].timestamp_ms = start_time;
            g_readings[SENSOR_TYPE_ADS1119_1].channels[ch].raw_value = raw;
            g_readings[SENSOR_TYPE_ADS1119_1].channels[ch].valid = (err == ESP_OK);
            
            if (err == ESP_OK) {
                const channel_conversion_t *conv = get_ads1119_1_conversion(ch);
                g_readings[SENSOR_TYPE_ADS1119_1].channels[ch].physical_value = 
                    apply_conversion(raw, conv);
            } else {
                g_readings[SENSOR_TYPE_ADS1119_1].error_count++;
            }
        }
        
        g_readings[SENSOR_TYPE_ADS1119_1].last_update_ms = start_time;
        g_acquisition_counts[SENSOR_TYPE_ADS1119_1]++;
        
        xSemaphoreGive(g_mutex[SENSOR_TYPE_ADS1119_1]);
        
        if (g_config.callback) {
            g_config.callback(SENSOR_TYPE_ADS1119_1, &g_readings[SENSOR_TYPE_ADS1119_1]);
        }
        
        if (g_config.enable_logging) {
            for (uint8_t ch = 0; ch < 4; ch++) {
                const channel_conversion_t *conv = get_ads1119_1_conversion(ch);
                if (conv && g_readings[SENSOR_TYPE_ADS1119_1].channels[ch].valid) {
                    ESP_LOGI(TAG, "ADS1119_1 CH%d [%s]: %.3f %s (raw=%ld)", 
                             ch, conv->name,
                             g_readings[SENSOR_TYPE_ADS1119_1].channels[ch].physical_value,
                             conv->unit,
                             g_readings[SENSOR_TYPE_ADS1119_1].channels[ch].raw_value);
                }
            }
        }
    }
}
#endif

#if ENABLE_ADS1119_2
static void ads1119_2_acquisition_task(void *arg) {
    const TickType_t period = pdMS_TO_TICKS(g_config.acquisition_period_ms);
    TickType_t last_wake = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "ADS1119 #2 acquisition task started");
    
    while (1) {
        vTaskDelayUntil(&last_wake, period);
        
        uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        xSemaphoreTake(g_mutex[SENSOR_TYPE_ADS1119_2], portMAX_DELAY);
        
        g_readings[SENSOR_TYPE_ADS1119_2].type = SENSOR_TYPE_ADS1119_2;
        g_readings[SENSOR_TYPE_ADS1119_2].num_channels = 4;
        
        for (uint8_t ch = 0; ch < 4; ch++) {
            int16_t raw = 0;
            esp_err_t err = ads1119_set_channel(&g_ads1119_2, ch);
            if (err == ESP_OK) {
                err = ads1119_read_raw16(&g_ads1119_2, &raw);
            }
            
            g_readings[SENSOR_TYPE_ADS1119_2].channels[ch].timestamp_ms = start_time;
            g_readings[SENSOR_TYPE_ADS1119_2].channels[ch].raw_value = raw;
            g_readings[SENSOR_TYPE_ADS1119_2].channels[ch].valid = (err == ESP_OK);
            
            if (err == ESP_OK) {
                const channel_conversion_t *conv = get_ads1119_2_conversion(ch);
                g_readings[SENSOR_TYPE_ADS1119_2].channels[ch].physical_value = 
                    apply_conversion(raw, conv);
            } else {
                g_readings[SENSOR_TYPE_ADS1119_2].error_count++;
            }
        }
        
        g_readings[SENSOR_TYPE_ADS1119_2].last_update_ms = start_time;
        g_acquisition_counts[SENSOR_TYPE_ADS1119_2]++;
        
        xSemaphoreGive(g_mutex[SENSOR_TYPE_ADS1119_2]);
        
        if (g_config.callback) {
            g_config.callback(SENSOR_TYPE_ADS1119_2, &g_readings[SENSOR_TYPE_ADS1119_2]);
        }
        
        if (g_config.enable_logging) {
            for (uint8_t ch = 0; ch < 4; ch++) {
                const channel_conversion_t *conv = get_ads1119_2_conversion(ch);
                if (conv && g_readings[SENSOR_TYPE_ADS1119_2].channels[ch].valid) {
                    ESP_LOGI(TAG, "ADS1119_2 CH%d [%s]: %.3f %s (raw=%ld)", 
                             ch, conv->name,
                             g_readings[SENSOR_TYPE_ADS1119_2].channels[ch].physical_value,
                             conv->unit,
                             g_readings[SENSOR_TYPE_ADS1119_2].channels[ch].raw_value);
                }
            }
        }
    }
}
#endif

/* ============================================================================
 * API PUBLIQUE
 * ============================================================================ */

esp_err_t sensor_manager_init(const sensor_manager_config_t *config) {
    if (g_initialized) {
        ESP_LOGW(TAG, "Sensor manager already initialized");
        return ESP_OK;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Configuration NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&g_config, config, sizeof(sensor_manager_config_t));
    ESP_LOGI(TAG, "Initializing sensor manager (period=%lu ms)", g_config.acquisition_period_ms);
    
    // Créer les mutex
    for (int i = 0; i < SENSOR_TYPE_COUNT; i++) {
        g_mutex[i] = xSemaphoreCreateMutex();
        if (!g_mutex[i]) {
            ESP_LOGE(TAG, "Failed to create mutex %d", i);
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Initialiser les ADC et créer les tâches
#if ENABLE_ADS7128
    ESP_LOGI(TAG, "Initializing ADS7128...");
    esp_err_t err = ads7128_init(&g_ads7128, I2C_NUM_0, ADS7128_I2C_ADDR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADS7128 init failed: %s", esp_err_to_name(err));
    } else {
        xTaskCreate(ads7128_acquisition_task, "ads7128_acq", 4096, NULL, 6, &g_tasks[SENSOR_TYPE_ADS7128]);
    }
#endif

#if ENABLE_ADS1119_1
    ESP_LOGI(TAG, "Initializing ADS1119 #1...");
    err = ads1119_init(&g_ads1119_1, I2C_NUM_0, ADS1119_1_I2C_ADDR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADS1119 #1 init failed: %s", esp_err_to_name(err));
    } else {
        xTaskCreate(ads1119_1_acquisition_task, "ads1119_1_acq", 4096, NULL, 6, &g_tasks[SENSOR_TYPE_ADS1119_1]);
    }
#endif

#if ENABLE_ADS1119_2
    ESP_LOGI(TAG, "Initializing ADS1119 #2...");
    err = ads1119_init(&g_ads1119_2, I2C_NUM_0, ADS1119_2_I2C_ADDR);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADS1119 #2 init failed: %s", esp_err_to_name(err));
    } else {
        xTaskCreate(ads1119_2_acquisition_task, "ads1119_2_acq", 4096, NULL, 6, &g_tasks[SENSOR_TYPE_ADS1119_2]);
    }
#endif
    
    g_initialized = true;
    ESP_LOGI(TAG, "Sensor manager initialized successfully");
    return ESP_OK;
}

esp_err_t sensor_manager_get_readings(sensor_type_t type, sensor_readings_t *readings) {
    if (!g_initialized || type >= SENSOR_TYPE_COUNT || !readings) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_mutex[type], portMAX_DELAY);
    memcpy(readings, &g_readings[type], sizeof(sensor_readings_t));
    xSemaphoreGive(g_mutex[type]);
    
    return ESP_OK;
}

esp_err_t sensor_manager_get_channel_value(sensor_type_t type, uint8_t channel, float *value) {
    if (!g_initialized || type >= SENSOR_TYPE_COUNT || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_mutex[type], portMAX_DELAY);
    if (channel < g_readings[type].num_channels && g_readings[type].channels[channel].valid) {
        *value = g_readings[type].channels[channel].physical_value;
        xSemaphoreGive(g_mutex[type]);
        return ESP_OK;
    }
    xSemaphoreGive(g_mutex[type]);
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sensor_manager_get_channel_info(sensor_type_t type, uint8_t channel,
                                           const char **name, const char **unit) {
    const channel_conversion_t *conv = NULL;
    
    switch (type) {
        case SENSOR_TYPE_ADS7128:
            conv = get_ads7128_conversion(channel);
            break;
        case SENSOR_TYPE_ADS1119_1:
            conv = get_ads1119_1_conversion(channel);
            break;
        case SENSOR_TYPE_ADS1119_2:
            conv = get_ads1119_2_conversion(channel);
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }
    
    if (conv) {
        if (name) *name = conv->name;
        if (unit) *unit = conv->unit;
        return ESP_OK;
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sensor_manager_get_stats(sensor_type_t type, 
                                    uint32_t *total_acquisitions,
                                    uint32_t *error_count,
                                    uint32_t *last_update_ms) {
    if (!g_initialized || type >= SENSOR_TYPE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(g_mutex[type], portMAX_DELAY);
    if (total_acquisitions) *total_acquisitions = g_acquisition_counts[type];
    if (error_count) *error_count = g_readings[type].error_count;
    if (last_update_ms) *last_update_ms = g_readings[type].last_update_ms;
    xSemaphoreGive(g_mutex[type]);
    
    return ESP_OK;
}

int sensor_manager_format_json(sensor_type_t type, char *json_buffer, size_t buffer_size) {
    if (!g_initialized || type >= SENSOR_TYPE_COUNT || !json_buffer) {
        return -1;
    }
    
    xSemaphoreTake(g_mutex[type], portMAX_DELAY);
    
    int pos = snprintf(json_buffer, buffer_size, 
                       "{\"node_id\":%d,\"sensor\":%d,\"timestamp\":%lu,\"channels\":[",
                       NODE_ID, type, g_readings[type].last_update_ms);
    
    for (uint8_t ch = 0; ch < g_readings[type].num_channels; ch++) {
        if (g_readings[type].channels[ch].valid) {
            const char *name = NULL, *unit = NULL;
            sensor_manager_get_channel_info(type, ch, &name, &unit);
            
            pos += snprintf(json_buffer + pos, buffer_size - pos,
                            "%s{\"ch\":%d,\"name\":\"%s\",\"value\":%.3f,\"unit\":\"%s\",\"raw\":%ld}",
                            ch > 0 ? "," : "", ch, name ? name : "unknown",
                            g_readings[type].channels[ch].physical_value,
                            unit ? unit : "?",
                            g_readings[type].channels[ch].raw_value);
        }
    }
    
    pos += snprintf(json_buffer + pos, buffer_size - pos, "]}");
    
    xSemaphoreGive(g_mutex[type]);
    
    return pos;
}

void sensor_manager_set_logging(bool enable) {
    g_config.enable_logging = enable;
}

void sensor_manager_deinit(void) {
    if (!g_initialized) return;
    
    // Supprimer les tâches
    for (int i = 0; i < SENSOR_TYPE_COUNT; i++) {
        if (g_tasks[i]) {
            vTaskDelete(g_tasks[i]);
            g_tasks[i] = NULL;
        }
        if (g_mutex[i]) {
            vSemaphoreDelete(g_mutex[i]);
            g_mutex[i] = NULL;
        }
    }
    
    g_initialized = false;
    ESP_LOGI(TAG, "Sensor manager deinitialized");
}
