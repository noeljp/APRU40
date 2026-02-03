/**
 * @file main_secured.c
 * @brief Application principale APRU40 avec sécurité renforcée
 * 
 * NOUVELLES FONCTIONNALITÉS DE SÉCURITÉ :
 * - Tamper switch avec effacement auto NVS + alerte
 * - Bluetooth avec whitelist MAC stricte (1 scanner)
 * - PIN aléatoire généré au boot
 * - Alertes sécurité vers gateway (ESP-NOW/MQTT)
 * 
 * Conformité NIS2 : Article 21 - Gestion des risques de cybersécurité
 * 
 * ⚠️ UTILISATION :
 * 1. Renommer ce fichier en main.c (backup l'ancien)
 * 2. Configurer TAMPER_GPIO et BT_SCANNER_MAC_WHITELIST dans node_config.h
 * 3. Compiler et flasher sur ESP32
 * 4. Noter le PIN affiché dans les logs pour pairing scanner
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

// Configuration et composants
#include "node_config.h"
#include "i2c_bus.h"
#include "sensor_manager.h"
#include "esp_now_secure.h"

// ✅ NOUVEAUX COMPOSANTS SÉCURITÉ
#include "tamper_security.h"
#include "bluetooth_security.h"

#if ENABLE_TCA9537
#include "tca9537.h"
#endif

#if NODE_MODE == MODE_GATEWAY && ENABLE_ETHERNET
#include "mqtt_manager.h"
#include "esp_eth.h"
#include "esp_netif.h"
#endif

#define TAG "APRU40_SEC"

/* ============================================================================
 * VARIABLES GLOBALES
 * ============================================================================ */

#if ENABLE_TCA9537
static tca9537_t g_gpio_expander;
#define PIN_HEARTBEAT   0   // LED heartbeat sur GPIO0
#define PIN_SCANNER_LED 1   // LED état scanner sur GPIO1
#endif

static const uint8_t g_aes_key[32] = AES_KEY;
static const uint8_t g_hmac_key[32] = HMAC_KEY;
static const uint8_t g_scanner_mac[6] = BT_SCANNER_MAC_WHITELIST;

/* ============================================================================
 * CALLBACKS SÉCURITÉ
 * ============================================================================ */

/**
 * @brief Callback alerte tamper (avant effacement NVS)
 * 
 * Appelé AVANT l'effacement NVS pour permettre envoi d'alerte.
 */
static void tamper_alert_callback(void) {
    ESP_LOGE(TAG, "🚨 TAMPER DÉTECTÉ - Envoi alerte...");
    
#if NODE_MODE == MODE_NODE
    // Nœud → Envoyer alerte ESP-NOW vers gateway
    typedef struct {
        uint8_t node_id;
        uint8_t alert_type;  // 1 = tamper, 2 = bt_unauthorized
        uint32_t timestamp;
    } security_alert_t;
    
    security_alert_t alert = {
        .node_id = NODE_ID,
        .alert_type = 1,  // Tamper
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000000)
    };
    
    // Broadcast alerte (adresse FF:FF:FF:FF:FF:FF)
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_secure_send(broadcast_mac, (uint8_t*)&alert, sizeof(alert));
    
    ESP_LOGW(TAG, "Alerte tamper envoyée (ESP-NOW)");
    
#elif NODE_MODE == MODE_GATEWAY && ENABLE_MQTT
    // Gateway → Publier alerte MQTT
    char alert_topic[64];
    snprintf(alert_topic, sizeof(alert_topic), "apru40/alert/tamper/%s", NODE_NAME);
    
    char alert_payload[128];
    snprintf(alert_payload, sizeof(alert_payload), 
             "{\"type\":\"tamper\",\"node\":\"%s\",\"timestamp\":%llu}",
             NODE_NAME, esp_timer_get_time() / 1000000);
    
    mqtt_manager_publish(alert_topic, alert_payload, strlen(alert_payload), 2, false);
    
    ESP_LOGW(TAG, "Alerte tamper publiée (MQTT)");
#endif
}

/**
 * @brief Callback tentative connexion Bluetooth non autorisée
 */
static void bluetooth_unauthorized_callback(const uint8_t *remote_mac) {
    ESP_LOGE(TAG, "🚫 Tentative connexion BT non autorisée: %02X:%02X:%02X:%02X:%02X:%02X",
             remote_mac[0], remote_mac[1], remote_mac[2], 
             remote_mac[3], remote_mac[4], remote_mac[5]);
    
#if NODE_MODE == MODE_NODE
    // Envoyer alerte ESP-NOW vers gateway
    typedef struct {
        uint8_t node_id;
        uint8_t alert_type;
        uint8_t unauthorized_mac[6];
        uint32_t timestamp;
    } security_alert_t;
    
    security_alert_t alert = {
        .node_id = NODE_ID,
        .alert_type = 2,  // BT unauthorized
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000000)
    };
    memcpy(alert.unauthorized_mac, remote_mac, 6);
    
    // Broadcast alerte
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_secure_send(broadcast_mac, (uint8_t*)&alert, sizeof(alert));
    
    ESP_LOGW(TAG, "Alerte BT unauthorized envoyée (ESP-NOW)");
#endif
}

/* ============================================================================
 * CALLBACKS BLUETOOTH SCANNER
 * ============================================================================ */

#if ENABLE_BLUETOOTH_SPP

/**
 * @brief Callback réception données du scanner Zebra DS2278
 */
static void scanner_data_callback(const char *data, size_t len) {
    ESP_LOGI(TAG, "📱 Scanner: %.*s", len, data);
    
    // Parser format: [TYPE]|[CODE]
    // Exemples: "TAS|123456", "EAN13|1234567890123", "QR|https://example.com"
    
    char type[16] = {0};
    char code[256] = {0};
    
    if (sscanf(data, "%15[^|]|%255s", type, code) == 2) {
        ESP_LOGI(TAG, "Type: %s, Code: %s", type, code);
        
        // TODO: Traiter le code scanné selon le type
        // - TAS: Numéro de tâche
        // - EAN13/EAN8: Code-barres produit
        // - QR: URL ou données structurées
        
        // Clignoter LED scanner (si TCA9537 actif)
#if ENABLE_TCA9537
        tca9537_write_pin(&g_gpio_expander, PIN_SCANNER_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        tca9537_write_pin(&g_gpio_expander, PIN_SCANNER_LED, 0);
#endif
    }
}

#endif // ENABLE_BLUETOOTH_SPP

/* ============================================================================
 * CALLBACKS ESP-NOW
 * ============================================================================ */

static void esp_now_recv_callback(const uint8_t *mac, const uint8_t *data, size_t len) {
#if NODE_MODE == MODE_GATEWAY
    ESP_LOGI(TAG, "📥 ESP-NOW reçu de %02X:%02X:%02X:%02X:%02X:%02X (%d bytes)",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len);
    
    // TODO: Traiter données reçues (publier vers MQTT si gateway)
#endif
}

static void esp_now_send_callback(const uint8_t *mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "✅ ESP-NOW envoyé avec succès");
    } else {
        ESP_LOGW(TAG, "❌ Échec envoi ESP-NOW");
    }
}

/* ============================================================================
 * TÂCHES FREERTOS
 * ============================================================================ */

/**
 * @brief Tâche heartbeat (LED + logs)
 */
static void heartbeat_task(void *pvParameters) {
    uint32_t counter = 0;
    
    while (1) {
        // Clignoter LED heartbeat
#if ENABLE_TCA9537
        tca9537_write_pin(&g_gpio_expander, PIN_HEARTBEAT, counter % 2);
#endif
        
        // Log périodique
        if (counter % 10 == 0) {
            ESP_LOGI(TAG, "💓 Heartbeat %lu | Free heap: %lu bytes", 
                     counter, esp_get_free_heap_size());
            
            // Statistiques Bluetooth sécurité
#if ENABLE_BLUETOOTH_SPP
            uint32_t total_conn, rejected_conn;
            bt_security_get_stats(&total_conn, &rejected_conn);
            if (rejected_conn > 0) {
                ESP_LOGW(TAG, "📊 BT Stats: %lu connexions, %lu refusées", 
                         total_conn, rejected_conn);
            }
#endif
            
            // État tamper
            if (tamper_security_is_triggered()) {
                ESP_LOGE(TAG, "⚠️ TAMPER ACTIF (déclenchements: %lu)", 
                         tamper_security_get_trigger_count());
            }
        }
        
        counter++;
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
}

/**
 * @brief Tâche envoi ESP-NOW (nœuds uniquement)
 */
#if NODE_MODE == MODE_NODE
static void esp_now_tx_task(void *pvParameters) {
    uint8_t gateway_mac[6] = GATEWAY_MAC_ADDR;
    
    while (1) {
        // Obtenir snapshot capteurs depuis sensor_manager
        sensor_snapshot_t snapshot;
        esp_err_t ret = sensor_manager_get_snapshot(&snapshot);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "📤 Envoi données vers gateway...");
            
            // Envoyer via ESP-NOW sécurisé
            ret = esp_now_secure_send(gateway_mac, (uint8_t*)&snapshot, sizeof(snapshot));
            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ Données envoyées (%d bytes)", sizeof(snapshot));
            } else {
                ESP_LOGE(TAG, "❌ Erreur envoi ESP-NOW: %s", esp_err_to_name(ret));
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(ESP_NOW_TX_PERIOD_MS));
    }
}
#endif

/* ============================================================================
 * APP_MAIN
 * ============================================================================ */

void app_main(void) {
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGI(TAG, "🚀 APRU40 SECURED - Démarrage");
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGI(TAG, "Node: %s (ID: %d)", NODE_NAME, NODE_ID);
    ESP_LOGI(TAG, "Mode: %s", NODE_MODE == MODE_NODE ? "NŒUD CAPTEUR" : "GATEWAY");
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    
    // ============================================================================
    // ÉTAPE 1 : SÉCURITÉ PHYSIQUE (TAMPER) - PRIORITÉ MAXIMALE
    // ============================================================================
    
    ESP_LOGI(TAG, "🔒 Initialisation sécurité physique (tamper)...");
    
    tamper_security_config_t tamper_config = {
        .tamper_gpio = TAMPER_GPIO,
        .active_low = TAMPER_ACTIVE_LOW,
        .auto_erase_nvs = TAMPER_AUTO_ERASE_NVS,
        .auto_restart = TAMPER_AUTO_RESTART,
        .debounce_ms = TAMPER_DEBOUNCE_MS,
        .alert_callback = tamper_alert_callback
    };
    
    esp_err_t ret = tamper_security_init(&tamper_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ ERREUR CRITIQUE : Impossible d'initialiser tamper security");
        ESP_LOGE(TAG, "⚠️ Le système continue SANS protection physique !");
        vTaskDelay(pdMS_TO_TICKS(3000));  // Pause pour lire le message
    } else {
        ESP_LOGI(TAG, "✅ Tamper security opérationnel");
    }
    
    // ============================================================================
    // ÉTAPE 2 : INITIALISATION NVS
    // ============================================================================
    
    ESP_LOGI(TAG, "💾 Initialisation NVS...");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrompue, effacement...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // ============================================================================
    // ÉTAPE 3 : INITIALISATION PÉRIPHÉRIQUES I2C
    // ============================================================================
    
    ESP_LOGI(TAG, "📡 Initialisation I2C bus...");
    ESP_ERROR_CHECK(i2c_bus_init());
    
#if ENABLE_TCA9537
    ESP_LOGI(TAG, "🔌 Initialisation TCA9537 (GPIO expander)...");
    tca9537_config_t gpio_config = {
        .i2c_addr = TCA9537_I2C_ADDR,
        .io_config = 0x00,  // Tous les pins en sortie
        .polarity_inv = 0x00
    };
    ESP_ERROR_CHECK(tca9537_init(&g_gpio_expander, &gpio_config));
    
    // Éteindre toutes les LEDs
    for (int i = 0; i < 4; i++) {
        tca9537_write_pin(&g_gpio_expander, i, 0);
    }
#endif
    
    // ============================================================================
    // ÉTAPE 4 : INITIALISATION SENSOR_MANAGER
    // ============================================================================
    
    ESP_LOGI(TAG, "📊 Initialisation sensor_manager...");
    ESP_ERROR_CHECK(sensor_manager_init());
    ESP_ERROR_CHECK(sensor_manager_start());
    
    // ============================================================================
    // ÉTAPE 5 : INITIALISATION ESP-NOW SÉCURISÉ
    // ============================================================================
    
    ESP_LOGI(TAG, "📡 Initialisation ESP-NOW sécurisé...");
    esp_now_secure_config_t espnow_config = {
        .channel = ESP_NOW_CHANNEL,
        .node_id = NODE_ID,
        .recv_cb = esp_now_recv_callback,
        .send_cb = esp_now_send_callback,
    };
    memcpy(espnow_config.aes_key, g_aes_key, 32);
    memcpy(espnow_config.hmac_key, g_hmac_key, 32);
    ESP_ERROR_CHECK(esp_now_secure_init(&espnow_config));
    
    // ============================================================================
    // ÉTAPE 6 : INITIALISATION BLUETOOTH SÉCURISÉ (nœuds uniquement)
    // ============================================================================
    
#if ENABLE_BLUETOOTH_SPP && NODE_MODE == MODE_NODE
    ESP_LOGI(TAG, "📱 Initialisation Bluetooth sécurisé...");
    
    bt_security_config_t bt_config = {
        .device_name = NODE_NAME,
        .scanner_mac = g_scanner_mac,
        .generate_random_pin = true,  // PIN aléatoire au boot
        .fixed_pin = NULL,
        .store_pin_nvs = true,  // Conserver PIN entre reboots
        .data_cb = scanner_data_callback,
        .unauthorized_cb = bluetooth_unauthorized_callback
    };
    
    ret = bt_security_init(&bt_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Erreur init Bluetooth security: %s", esp_err_to_name(ret));
    } else {
        // Afficher PIN pour opérateur (pour pairing scanner)
        char pin[BT_SEC_PIN_LENGTH + 1];
        bt_security_get_pin(pin);
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
        ESP_LOGI(TAG, "🔑 PIN BLUETOOTH POUR PAIRING SCANNER");
        ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
        ESP_LOGI(TAG, "   PIN: %s", pin);
        ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "⚠️ PROCÉDURE PAIRING :");
        ESP_LOGI(TAG, "1. Activer discoverable: bt_security_set_discoverable(true, 300)");
        ESP_LOGI(TAG, "2. Scanner le code-barres pairing Zebra DS2278");
        ESP_LOGI(TAG, "3. Entrer PIN: %s", pin);
        ESP_LOGI(TAG, "4. Discoverable se désactivera automatiquement après 5 min");
        ESP_LOGI(TAG, "");
        
        // TODO: Afficher PIN sur écran LED/LCD si disponible
    }
#endif
    
    // ============================================================================
    // ÉTAPE 7 : CRÉATION TÂCHES FREERTOS
    // ============================================================================
    
    ESP_LOGI(TAG, "🔄 Création des tâches FreeRTOS...");
    
    // Heartbeat (priorité basse)
    xTaskCreate(heartbeat_task, "heartbeat", 2048, NULL, 1, NULL);
    
#if NODE_MODE == MODE_NODE
    // Tâche d'envoi ESP-NOW (uniquement nœuds)
    xTaskCreate(esp_now_tx_task, "esp_now_tx", 4096, NULL, 5, NULL);
#endif
    
    // ============================================================================
    // SYSTÈME OPÉRATIONNEL
    // ============================================================================
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGI(TAG, "✅ SYSTÈME OPÉRATIONNEL");
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGI(TAG, "📊 Configuration:");
    ESP_LOGI(TAG, "  - ADS7128:        %s", ENABLE_ADS7128 ? "✅" : "❌");
    ESP_LOGI(TAG, "  - ADS1119 #1:     %s", ENABLE_ADS1119_1 ? "✅" : "❌");
    ESP_LOGI(TAG, "  - ADS1119 #2:     %s", ENABLE_ADS1119_2 ? "✅" : "❌");
    ESP_LOGI(TAG, "  - TCA9537:        %s", ENABLE_TCA9537 ? "✅" : "❌");
    ESP_LOGI(TAG, "  - Bluetooth:      %s", ENABLE_BLUETOOTH_SPP ? "✅ Sécurisé" : "❌");
    ESP_LOGI(TAG, "  - Tamper:         %s", "✅ Actif");
    ESP_LOGI(TAG, "  - Simulation:     %s", IS_SIMULATION ? "✅" : "❌");
    ESP_LOGI(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGI(TAG, "");
    
    // La boucle principale est gérée par FreeRTOS
    // Les acquisitions sont automatiques via sensor_manager
}
