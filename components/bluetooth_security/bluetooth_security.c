/**
 * @file bluetooth_security.c
 * @brief Implémentation sécurité Bluetooth avec whitelist MAC stricte
 */

#include "bluetooth_security.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdio.h>

#define TAG "BT_SEC"

#define NVS_NAMESPACE       "bt_security"
#define NVS_KEY_PIN         "pin"
#define NVS_KEY_SCANNER_MAC "scanner_mac"

static bt_security_config_t g_config = {0};
static uint8_t g_scanner_mac[6] = {0};
static char g_pin[BT_SEC_PIN_LENGTH + 1] = {0};
static uint32_t g_total_connections = 0;
static uint32_t g_rejected_connections = 0;
static TimerHandle_t g_discoverable_timer = NULL;

/* Forward declarations */
static void bt_connection_callback(bool connected, const uint8_t *remote_addr);
static esp_err_t load_pin_from_nvs(char *pin_out);
static esp_err_t save_pin_to_nvs(const char *pin);
static esp_err_t load_scanner_mac_from_nvs(uint8_t *mac_out);
static esp_err_t save_scanner_mac_to_nvs(const uint8_t *mac);
static void generate_random_pin(char *pin_out, size_t len);
static void discoverable_timer_callback(TimerHandle_t xTimer);

esp_err_t bt_security_init(const bt_security_config_t *config) {
    if (!config || !config->device_name || !config->scanner_mac) {
        ESP_LOGE(TAG, "Configuration invalide (device_name ou scanner_mac NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copier configuration
    memcpy(&g_config, config, sizeof(bt_security_config_t));
    memcpy(g_scanner_mac, config->scanner_mac, 6);
    
    // Initialiser NVS si nécessaire
    esp_err_t ret = nvs_flash_init_partition(NVS_DEFAULT_PART_NAME);
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase, reinitializing...");
        nvs_flash_erase_partition(NVS_DEFAULT_PART_NAME);
        ret = nvs_flash_init_partition(NVS_DEFAULT_PART_NAME);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init NVS: %s", esp_err_to_name(ret));
    }
    
    // === GÉNÉRATION/CHARGEMENT PIN ===
    
    if (config->generate_random_pin) {
        // Tenter de charger PIN depuis NVS
        ret = load_pin_from_nvs(g_pin);
        
        if (ret != ESP_OK) {
            // PIN pas en NVS → générer nouveau PIN aléatoire
            ESP_LOGI(TAG, "Génération nouveau PIN aléatoire...");
            generate_random_pin(g_pin, BT_SEC_PIN_LENGTH);
            
            // Sauvegarder en NVS si demandé
            if (config->store_pin_nvs) {
                save_pin_to_nvs(g_pin);
            }
        } else {
            ESP_LOGI(TAG, "PIN chargé depuis NVS");
        }
    } else {
        // PIN fixe fourni par utilisateur
        if (!config->fixed_pin || strlen(config->fixed_pin) < 4) {
            ESP_LOGE(TAG, "PIN fixe invalide (min 4 chiffres)");
            return ESP_ERR_INVALID_ARG;
        }
        strncpy(g_pin, config->fixed_pin, BT_SEC_PIN_LENGTH);
        g_pin[BT_SEC_PIN_LENGTH] = '\0';
    }
    
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGI(TAG, "🔐 BLUETOOTH SECURITY - Configuration");
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGI(TAG, "📱 Device:      %s", config->device_name);
    ESP_LOGI(TAG, "🔑 PIN:         %s", g_pin);
    ESP_LOGI(TAG, "📡 Scanner MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             g_scanner_mac[0], g_scanner_mac[1], g_scanner_mac[2],
             g_scanner_mac[3], g_scanner_mac[4], g_scanner_mac[5]);
    ESP_LOGI(TAG, "🔒 Whitelist:   STRICTE (1 scanner autorisé)");
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    
    // === CHARGEMENT/SAUVEGARDE MAC SCANNER ===
    
    uint8_t nvs_mac[6];
    ret = load_scanner_mac_from_nvs(nvs_mac);
    if (ret == ESP_OK) {
        // Vérifier si MAC a changé
        if (memcmp(nvs_mac, g_scanner_mac, 6) != 0) {
            ESP_LOGW(TAG, "⚠️ MAC scanner différente de NVS, mise à jour...");
            save_scanner_mac_to_nvs(g_scanner_mac);
        }
    } else {
        // Première config → sauvegarder MAC
        save_scanner_mac_to_nvs(g_scanner_mac);
    }
    
    // === INITIALISATION BLUETOOTH SPP ===
    
    bt_spp_config_t spp_config = {
        .device_name = config->device_name,
        .pin_code = g_pin,
        .discoverable_at_init = false,  // Désactivé par défaut (sécurité)
        .whitelist_addr = g_scanner_mac,
        .data_cb = config->data_cb,
        .conn_cb = bt_connection_callback  // Notre callback avec vérification
    };
    
    ret = bt_spp_init(&spp_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init Bluetooth SPP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Bluetooth Security initialisé avec succès");
    ESP_LOGI(TAG, "⚠️ Pairing: Activer discoverable avec bt_security_set_discoverable(true, 300)");
    
    return ESP_OK;
}

esp_err_t bt_security_get_pin(char *pin_out) {
    if (!pin_out) return ESP_ERR_INVALID_ARG;
    strncpy(pin_out, g_pin, BT_SEC_PIN_LENGTH + 1);
    return ESP_OK;
}

bool bt_security_is_authorized(const uint8_t *remote_mac) {
    if (!remote_mac) return false;
    return memcmp(remote_mac, g_scanner_mac, 6) == 0;
}

esp_err_t bt_security_get_scanner_mac(uint8_t *mac_out) {
    if (!mac_out) return ESP_ERR_INVALID_ARG;
    memcpy(mac_out, g_scanner_mac, 6);
    return ESP_OK;
}

esp_err_t bt_security_update_scanner_mac(const uint8_t *new_mac, bool store_nvs) {
    if (!new_mac) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Mise à jour MAC scanner: %02X:%02X:%02X:%02X:%02X:%02X",
             new_mac[0], new_mac[1], new_mac[2], new_mac[3], new_mac[4], new_mac[5]);
    
    memcpy(g_scanner_mac, new_mac, 6);
    
    if (store_nvs) {
        return save_scanner_mac_to_nvs(new_mac);
    }
    
    ESP_LOGW(TAG, "⚠️ Redémarrage Bluetooth requis pour appliquer nouvelle MAC");
    return ESP_OK;
}

esp_err_t bt_security_regenerate_pin(bool store_nvs) {
    ESP_LOGI(TAG, "Régénération PIN aléatoire...");
    generate_random_pin(g_pin, BT_SEC_PIN_LENGTH);
    
    ESP_LOGI(TAG, "Nouveau PIN: %s", g_pin);
    
    if (store_nvs) {
        return save_pin_to_nvs(g_pin);
    }
    
    ESP_LOGW(TAG, "⚠️ Redémarrage Bluetooth requis pour appliquer nouveau PIN");
    return ESP_OK;
}

esp_err_t bt_security_set_discoverable(bool enable, uint32_t timeout_sec) {
    esp_err_t ret = bt_spp_set_discoverable(enable);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (enable && timeout_sec > 0) {
        // Créer timer pour désactivation automatique
        if (g_discoverable_timer == NULL) {
            g_discoverable_timer = xTimerCreate(
                "bt_disc_timeout",
                pdMS_TO_TICKS(timeout_sec * 1000),
                pdFALSE,  // One-shot
                NULL,
                discoverable_timer_callback
            );
        }
        
        if (g_discoverable_timer) {
            xTimerStart(g_discoverable_timer, 0);
            ESP_LOGI(TAG, "🕐 Discoverable activé pour %lu secondes", timeout_sec);
        }
    }
    
    return ESP_OK;
}

esp_err_t bt_security_get_stats(uint32_t *total_connections_out, uint32_t *rejected_connections_out) {
    if (total_connections_out) *total_connections_out = g_total_connections;
    if (rejected_connections_out) *rejected_connections_out = g_rejected_connections;
    return ESP_OK;
}

void bt_security_deinit(void) {
    if (g_discoverable_timer) {
        xTimerDelete(g_discoverable_timer, 0);
        g_discoverable_timer = NULL;
    }
    bt_spp_deinit();
    ESP_LOGI(TAG, "Bluetooth Security désinitialisé");
}

/* ===== Fonctions internes ===== */

/**
 * @brief Callback connexion Bluetooth avec vérification sécurité
 */
static void bt_connection_callback(bool connected, const uint8_t *remote_addr) {
    if (connected) {
        // Vérifier whitelist (double vérification au niveau application)
        if (!bt_security_is_authorized(remote_addr)) {
            ESP_LOGE(TAG, "🚫 CONNEXION REFUSÉE : MAC non autorisé");
            ESP_LOGE(TAG, "   Device: %02X:%02X:%02X:%02X:%02X:%02X",
                     remote_addr[0], remote_addr[1], remote_addr[2],
                     remote_addr[3], remote_addr[4], remote_addr[5]);
            ESP_LOGE(TAG, "   Attendu: %02X:%02X:%02X:%02X:%02X:%02X",
                     g_scanner_mac[0], g_scanner_mac[1], g_scanner_mac[2],
                     g_scanner_mac[3], g_scanner_mac[4], g_scanner_mac[5]);
            
            g_rejected_connections++;
            
            // Callback utilisateur (pour alerte MQTT/ESP-NOW)
            if (g_config.unauthorized_cb) {
                g_config.unauthorized_cb(remote_addr);
            }
            
            // Déconnecter immédiatement
            bt_spp_disconnect();
            return;
        }
        
        g_total_connections++;
        ESP_LOGI(TAG, "✅ Connexion autorisée (scanner Zebra)");
        ESP_LOGI(TAG, "   Total connexions: %lu", g_total_connections);
    } else {
        ESP_LOGI(TAG, "📴 Scanner déconnecté");
    }
}

/**
 * @brief Générer PIN aléatoire 6 chiffres
 */
static void generate_random_pin(char *pin_out, size_t len) {
    uint32_t random = esp_random() % 1000000;  // 0-999999
    snprintf(pin_out, len + 1, "%06lu", random);
}

/**
 * @brief Charger PIN depuis NVS
 */
static esp_err_t load_pin_from_nvs(char *pin_out) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t len = BT_SEC_PIN_LENGTH + 1;
    ret = nvs_get_str(nvs_handle, NVS_KEY_PIN, pin_out, &len);
    nvs_close(nvs_handle);
    
    return ret;
}

/**
 * @brief Sauvegarder PIN en NVS
 */
static esp_err_t save_pin_to_nvs(const char *pin) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur ouverture NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_str(nvs_handle, NVS_KEY_PIN, pin);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "PIN sauvegardé en NVS");
        }
    }
    
    nvs_close(nvs_handle);
    return ret;
}

/**
 * @brief Charger MAC scanner depuis NVS
 */
static esp_err_t load_scanner_mac_from_nvs(uint8_t *mac_out) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t len = 6;
    ret = nvs_get_blob(nvs_handle, NVS_KEY_SCANNER_MAC, mac_out, &len);
    nvs_close(nvs_handle);
    
    return ret;
}

/**
 * @brief Sauvegarder MAC scanner en NVS
 */
static esp_err_t save_scanner_mac_to_nvs(const uint8_t *mac) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur ouverture NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_blob(nvs_handle, NVS_KEY_SCANNER_MAC, mac, 6);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "MAC scanner sauvegardée en NVS");
        }
    }
    
    nvs_close(nvs_handle);
    return ret;
}

/**
 * @brief Callback timer discoverable timeout
 */
static void discoverable_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "⏱️ Timeout discoverable → Désactivation automatique");
    bt_spp_set_discoverable(false);
}
