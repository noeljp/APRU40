/**
 * @file tamper_security.c
 * @brief Implémentation de la sécurité physique par tamper switch
 */

#include "tamper_security.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>

#define TAG "TAMPER_SEC"

static tamper_security_config_t g_config = {0};
static volatile bool g_tamper_triggered = false;
static volatile uint32_t g_trigger_count = 0;
static volatile bool g_enabled = false;
static TimerHandle_t g_debounce_timer = NULL;

/* Forward declarations */
static void IRAM_ATTR tamper_isr_handler(void* arg);
static void debounce_timer_callback(TimerHandle_t xTimer);
static void tamper_reaction(void);

esp_err_t tamper_security_init(const tamper_security_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Configuration NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Valider GPIO (doit être input-capable)
    if (!GPIO_IS_VALID_GPIO(config->tamper_gpio)) {
        ESP_LOGE(TAG, "GPIO invalide: %d", config->tamper_gpio);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copier configuration
    memcpy(&g_config, config, sizeof(tamper_security_config_t));
    
    // Configuration GPIO en entrée avec pull-up/pull-down
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->tamper_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = config->active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = config->active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE  // Détection sur changement d'état
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur config GPIO %d: %s", config->tamper_gpio, esp_err_to_name(ret));
        return ret;
    }
    
    // Créer timer anti-rebond
    g_debounce_timer = xTimerCreate(
        "tamper_debounce",
        pdMS_TO_TICKS(config->debounce_ms > 0 ? config->debounce_ms : 50),
        pdFALSE,  // One-shot
        NULL,
        debounce_timer_callback
    );
    
    if (g_debounce_timer == NULL) {
        ESP_LOGE(TAG, "Erreur création timer debounce");
        return ESP_ERR_NO_MEM;
    }
    
    // Installer ISR handler
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {  // ESP_ERR_INVALID_STATE = déjà installé
        ESP_LOGE(TAG, "Erreur install ISR service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = gpio_isr_handler_add(config->tamper_gpio, tamper_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur add ISR handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    g_enabled = true;
    
    ESP_LOGI(TAG, "Tamper security initialisé");
    ESP_LOGI(TAG, "  GPIO:          %d", config->tamper_gpio);
    ESP_LOGI(TAG, "  Active Low:    %s", config->active_low ? "OUI" : "NON");
    ESP_LOGI(TAG, "  Auto-erase:    %s", config->auto_erase_nvs ? "OUI" : "NON");
    ESP_LOGI(TAG, "  Auto-restart:  %s", config->auto_restart ? "OUI" : "NON");
    ESP_LOGI(TAG, "  Debounce:      %lu ms", config->debounce_ms);
    
    // Vérifier état initial (boîtier déjà ouvert ?)
    int level = gpio_get_level(config->tamper_gpio);
    bool is_tampered = (config->active_low) ? (level == 0) : (level == 1);
    
    if (is_tampered) {
        ESP_LOGW(TAG, "⚠️ TAMPER DÉTECTÉ AU BOOT ! Boîtier ouvert.");
        tamper_reaction();
    } else {
        ESP_LOGI(TAG, "✅ État tamper OK (boîtier fermé)");
    }
    
    return ESP_OK;
}

bool tamper_security_is_triggered(void) {
    return g_tamper_triggered;
}

uint32_t tamper_security_get_trigger_count(void) {
    return g_trigger_count;
}

esp_err_t tamper_security_test(void) {
    ESP_LOGW(TAG, "🧪 TEST TAMPER MANUEL - Simulation déclenchement");
    tamper_reaction();
    return ESP_OK;
}

esp_err_t tamper_security_disable(void) {
    if (!g_enabled) {
        return ESP_OK;
    }
    
    gpio_intr_disable(g_config.tamper_gpio);
    g_enabled = false;
    ESP_LOGW(TAG, "🔓 Tamper DÉSACTIVÉ (maintenance)");
    
    return ESP_OK;
}

esp_err_t tamper_security_enable(void) {
    if (g_enabled) {
        return ESP_OK;
    }
    
    gpio_intr_enable(g_config.tamper_gpio);
    g_enabled = true;
    ESP_LOGI(TAG, "🔒 Tamper RÉACTIVÉ");
    
    return ESP_OK;
}

void tamper_security_deinit(void) {
    if (g_enabled) {
        gpio_intr_disable(g_config.tamper_gpio);
        gpio_isr_handler_remove(g_config.tamper_gpio);
    }
    
    if (g_debounce_timer) {
        xTimerDelete(g_debounce_timer, 0);
        g_debounce_timer = NULL;
    }
    
    g_enabled = false;
    ESP_LOGI(TAG, "Tamper security désinitialisé");
}

/* ===== Fonctions internes ===== */

/**
 * @brief ISR appelé sur changement d'état GPIO tamper
 * 
 * IRAM_ATTR = exécuté depuis RAM (plus rapide, nécessaire pour ISR)
 */
static void IRAM_ATTR tamper_isr_handler(void* arg) {
    // Démarrer timer anti-rebond (confirmation après debounce_ms)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerStartFromISR(g_debounce_timer, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Callback timer anti-rebond
 * 
 * Appelé après debounce_ms pour confirmer le tamper (pas un rebond).
 */
static void debounce_timer_callback(TimerHandle_t xTimer) {
    // Vérifier que l'état est toujours "tamper détecté"
    int level = gpio_get_level(g_config.tamper_gpio);
    bool is_tampered = (g_config.active_low) ? (level == 0) : (level == 1);
    
    if (is_tampered && g_enabled) {
        ESP_LOGE(TAG, "🚨 TAMPER CONFIRMÉ après debounce");
        tamper_reaction();
    }
}

/**
 * @brief Réaction au tamper : effacement NVS + alerte + restart
 */
static void tamper_reaction(void) {
    g_tamper_triggered = true;
    g_trigger_count++;
    
    ESP_LOGE(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGE(TAG, "🚨 ALERTE SÉCURITÉ : TAMPER DÉTECTÉ (#%lu)", g_trigger_count);
    ESP_LOGE(TAG, "═══════════════════════════════════════════════════");
    
    // Désactiver interruption tamper (éviter déclenchements multiples)
    gpio_intr_disable(g_config.tamper_gpio);
    
    // 1. Callback utilisateur (pour envoyer alerte MQTT/ESP-NOW AVANT effacement)
    if (g_config.alert_callback) {
        ESP_LOGW(TAG, "⚠️ Envoi alerte tamper...");
        g_config.alert_callback();
        vTaskDelay(pdMS_TO_TICKS(500));  // Laisser temps d'envoi
    }
    
    // 2. Effacer NVS (clés, certificats, configuration sensible)
    if (g_config.auto_erase_nvs) {
        ESP_LOGW(TAG, "🗑️ EFFACEMENT NVS...");
        esp_err_t ret = nvs_flash_erase();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ NVS effacé avec succès");
        } else {
            ESP_LOGE(TAG, "❌ Erreur effacement NVS: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "⚠️ Effacement NVS désactivé (mode debug)");
    }
    
    ESP_LOGE(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGE(TAG, "🔴 DISPOSITIF COMPROMIS - REDÉMARRAGE IMMINENT");
    ESP_LOGE(TAG, "═══════════════════════════════════════════════════");
    
    // 3. Redémarrage sécurisé
    if (g_config.auto_restart) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 seconde pour afficher logs
        esp_restart();
    }
}
