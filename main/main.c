/**
 * @file main.c (VERSION SIMPLIFIÉE AVEC SENSOR_MANAGER)
 * @brief Application principale APRU40 - Mode nœud/gateway configurable
 * 
 * Architecture modulaire :
 * - node_config.h : Configuration centralisée (mode, périphériques, réseau)
 * - conversion_config.h : Lois de conversion ADC → physique
 * - sensor_manager : Acquisition automatique en tâche de fond
 * - esp_now_secure : Communication sans fil chiffrée
 * 
 * MODE_NODE (Nœuds capteurs) :
 *   - Acquisition capteurs (ADS7128, ADS1119) via sensor_manager
 *   - Scanner Zebra DS2278 via Bluetooth SPP (optionnel)
 *   - Transmission vers gateway via ESP-NOW (AES + HMAC)
 * 
 * MODE_GATEWAY (Passerelles) :
 *   - Réception données nœuds via ESP-NOW
 *   - Publication vers cloud via Ethernet + MQTT/TLS 1.3 (Mosquitto)
 *   - Réception commandes/configurations via MQTT
 *   - PAS de Bluetooth (uniquement sur nœuds)
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

#if ENABLE_TCA9537
#include "tca9537.h"
#endif

#if ENABLE_BLUETOOTH_SPP
#include "bluetooth_spp.h"
#endif

#if NODE_MODE == MODE_GATEWAY && ENABLE_ETHERNET
#include "mqtt_manager.h"
#include "esp_eth.h"
#include "esp_netif.h"
#endif

#define TAG "APRU40"

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

/* ============================================================================
 * CALLBACKS BLUETOOTH SCANNER
 * ============================================================================ */

#if ENABLE_BLUETOOTH_SPP

/**
 * @brief Callback réception données du scanner Zebra DS2278 (ligne par ligne)
 * 
 * Le scanner envoie les codes scannés avec suffixe CR+LF (\r\n)
 * Format attendu : [TYPE]|[CODE]
 * Exemples : "TAS|123456", "EAN13|1234567890123", "QR|https://example.com"
 */
static void scanner_data_callback(const char *data, size_t len) {
    ESP_LOGI(TAG, "📱 Scanner: %.*s", len, data);
    
    // Parser les différents formats de codes
    if (strncmp((char*)data, "TAS|", 4) == 0) {
        ESP_LOGI(TAG, "  ➜ Type: TAS, Code: %.*s", len-4, data+4);
    }
    else if (strncmp((char*)data, "EAN13|", 6) == 0) {
        ESP_LOGI(TAG, "  ➜ Type: EAN13, Code: %.*s", len-6, data+6);
    }
    else if (strncmp((char*)data, "QR|", 3) == 0) {
        ESP_LOGI(TAG, "  ➜ Type: QR Code, Contenu: %.*s", len-3, data+3);
    }
    else {
        ESP_LOGI(TAG, "  ➜ Format inconnu");
    }
    
    // Envoyer via ESP-NOW si c'est un nœud
#if NODE_MODE == MODE_NODE
    esp_now_secure_send((const uint8_t*)data, len);
#endif
}

/**
 * @brief Callback événements de connexion/déconnexion scanner Zebra DS2278
 * 
 * Gère l'état de la LED indicateur de connexion scanner
 */
static void scanner_conn_callback(bool connected, const uint8_t *remote_addr) {
    if (connected) {
        ESP_LOGI(TAG, "✅ Scanner connecté: %02X:%02X:%02X:%02X:%02X:%02X",
                 remote_addr[0], remote_addr[1], remote_addr[2],
                 remote_addr[3], remote_addr[4], remote_addr[5]);
        
#if ENABLE_TCA9537
        tca9537_set_pin(&g_gpio_expander, PIN_SCANNER_LED);
#endif
    } else {
        ESP_LOGW(TAG, "❌ Scanner déconnecté");
        
#if ENABLE_TCA9537
        tca9537_clear_pin(&g_gpio_expander, PIN_SCANNER_LED);
#endif
    }
}

#endif // ENABLE_BLUETOOTH_SPP

/* ============================================================================
 * CALLBACKS ESP-NOW
 * ============================================================================ */

/**
 * @brief Callback réception données ESP-NOW (gateway ou commandes vers nœud)
 * 
 * MODE_GATEWAY : 
 *   - Reçoit données capteurs depuis nœuds (format JSON ou binaire)
 *   - Agrège et publie vers broker Mosquitto via MQTT/TLS 1.3
 *   - Topic : apru40/gateway/{gateway_id}/data
 * 
 * MODE_NODE :
 *   - Reçoit commandes/configurations depuis gateway
 *   - Exemples : "LED_ON", "LED_OFF", config JSON
 */
static void esp_now_recv_callback(const uint8_t *sender_mac, 
                                  const uint8_t *data, 
                                  uint8_t len, 
                                  int8_t rssi) {
    ESP_LOGI(TAG, "📡 ESP-NOW RX de %02X:%02X:%02X:%02X:%02X:%02X: %d octets, RSSI=%d dBm",
             sender_mac[0], sender_mac[1], sender_mac[2], 
             sender_mac[3], sender_mac[4], sender_mac[5],
             len, rssi);
    
#if NODE_MODE == MODE_GATEWAY
    // Mode gateway : traiter les données des nœuds et publier via MQTT
    if (len > 0 && len < 250) {
        char buf[251] = {0};
        memcpy(buf, data, len);
        ESP_LOGI(TAG, "  ➜ Données: %s", buf);
        
        // TODO: Parser JSON et publier vers MQTT
        // Format attendu : {"node_id":1, "qr_code":"PROD123", "sensors":{...}}
#if ENABLE_ETHERNET
        // Publier vers broker Mosquitto
        // mqtt_manager_publish(MQTT_TOPIC_DATA, buf, len, 1, false);
        ESP_LOGD(TAG, "  (MQTT publication à implémenter)");
#endif
    }
#else
    // Mode nœud : traiter commandes reçues
    if (len > 0) {
        if (strncmp((char*)data, "LED_ON", 6) == 0) {
            ESP_LOGI(TAG, "  ➜ Commande: LED ON");
#if ENABLE_TCA9537
            tca9537_set_pin(&g_gpio_expander, PIN_HEARTBEAT);
#endif
        }
        else if (strncmp((char*)data, "LED_OFF", 7) == 0) {
            ESP_LOGI(TAG, "  ➜ Commande: LED OFF");
#if ENABLE_TCA9537
            tca9537_clear_pin(&g_gpio_expander, PIN_HEARTBEAT);
#endif
        }
    }
#endif
}

/**
 * @brief Callback statut envoi ESP-NOW
 */
static void esp_now_send_callback(const uint8_t *mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "ESP-NOW TX succès");
    } else {
        ESP_LOGW(TAG, "ESP-NOW TX échec");
    }
}

/* ============================================================================
 * CALLBACKS SENSOR MANAGER
 * ============================================================================ */

/**
 * @brief Callback appelé après chaque acquisition de capteur
 * 
 * Permet de traiter les données fraîches immédiatement :
 * - Vérification seuils d'alarme
 * - Calculs dérivés (moyennes, dérivées, etc.)
 * - Préparation paquets ESP-NOW pour transmission
 * - Association code QR scanner avec données capteurs
 */
static void sensor_acquisition_callback(sensor_type_t type, const sensor_readings_t *readings) {
    // Log de debug (optionnel, déjà géré par sensor_manager si logging activé)
    ESP_LOGD(TAG, "Capteur %d acquis : %lu canaux, erreurs=%lu", 
             type, readings->num_channels, readings->error_count);
    
    // Ici on peut ajouter des traitements spécifiques temps-réel
    // Par exemple : alarmes sur seuils, calculs dérivés, etc.
}

/* ============================================================================
 * TÂCHES FREERTOS
 * ============================================================================ */

/**
 * @brief Tâche heartbeat (LED témoin d'activité)
 */
static void heartbeat_task(void *arg) {
    ESP_LOGI(TAG, "Heartbeat task started");
    
#if ENABLE_TCA9537
    bool led_state = false;
    
    while (1) {
        led_state = !led_state;
        if (led_state) {
            tca9537_set_pin(&g_gpio_expander, PIN_HEARTBEAT);
        } else {
            tca9537_clear_pin(&g_gpio_expander, PIN_HEARTBEAT);
        }
        
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
#else
    // Pas de GPIO expander : juste un log périodique
    while (1) {
        ESP_LOGI(TAG, "❤️ Heartbeat");
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
#endif
}

#if NODE_MODE == MODE_NODE

/**
 * @brief Tâche d'envoi périodique des données capteurs via ESP-NOW (MODE_NODE uniquement)
 */
static void esp_now_tx_task(void *arg) {
    ESP_LOGI(TAG, "ESP-NOW TX task started (période=%lu ms)", ESP_NOW_TX_PERIOD_MS);
    
    char json_buffer[512];
    uint32_t seq = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(ESP_NOW_TX_PERIOD_MS));
        
        // Construire un JSON avec toutes les données des capteurs
        int pos = snprintf(json_buffer, sizeof(json_buffer), 
                           "{\"node\":%d,\"seq\":%lu,\"sensors\":[", 
                           NODE_ID, seq++);
        
        // Ajouter ADS7128
        if (ENABLE_ADS7128) {
            int len = sensor_manager_format_json(SENSOR_TYPE_ADS7128, 
                                                 json_buffer + pos, 
                                                 sizeof(json_buffer) - pos);
            if (len > 0) pos += len;
        }
        
        // Ajouter ADS1119 #1
        if (ENABLE_ADS1119_1 && pos < sizeof(json_buffer) - 100) {
            json_buffer[pos++] = ',';
            int len = sensor_manager_format_json(SENSOR_TYPE_ADS1119_1, 
                                                 json_buffer + pos, 
                                                 sizeof(json_buffer) - pos);
            if (len > 0) pos += len;
        }
        
        // Ajouter ADS1119 #2
        if (ENABLE_ADS1119_2 && pos < sizeof(json_buffer) - 100) {
            json_buffer[pos++] = ',';
            int len = sensor_manager_format_json(SENSOR_TYPE_ADS1119_2, 
                                                 json_buffer + pos, 
                                                 sizeof(json_buffer) - pos);
            if (len > 0) pos += len;
        }
        
        // Fermer le JSON
        pos += snprintf(json_buffer + pos, sizeof(json_buffer) - pos, "]}");
        
        // Envoyer via ESP-NOW sécurisé
        ESP_LOGI(TAG, "📤 Envoi ESP-NOW: %d octets", pos);
        ESP_LOGD(TAG, "  JSON: %s", json_buffer);
        
        esp_err_t err = esp_now_secure_send((uint8_t*)json_buffer, pos);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "  ❌ Erreur envoi: %s", esp_err_to_name(err));
        }
    }
}

#endif // MODE_NODE

/* ============================================================================
 * FONCTION PRINCIPALE
 * ============================================================================ */

void app_main(void) {
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "  APRU40 - Système de capteurs IoT");
    ESP_LOGI(TAG, "  Mode: %s", NODE_MODE == MODE_NODE ? "NŒUD" : "GATEWAY");
    ESP_LOGI(TAG, "  Node ID: %d", NODE_ID);
    ESP_LOGI(TAG, "  Nom: %s", NODE_NAME);
    ESP_LOGI(TAG, "==============================================");
    
    // Initialiser NVS (requis pour WiFi/Bluetooth)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialiser bus I2C matériel (ESP-IDF)
    ESP_LOGI(TAG, "Initialisation I2C...");
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    
    // Initialiser mutex I2C (i2c_bus)
    i2c_bus_init();
    
    // Initialiser GPIO expander TCA9537
#if ENABLE_TCA9537
    ESP_LOGI(TAG, "Initialisation TCA9537...");
    ESP_ERROR_CHECK(tca9537_init(&g_gpio_expander, I2C_NUM_0, TCA9537_I2C_ADDR));
    ESP_ERROR_CHECK(tca9537_set_pin_mode(&g_gpio_expander, TCA9537_ALL_PINS, false)); // Tous en sortie
    ESP_ERROR_CHECK(tca9537_clear_pin(&g_gpio_expander, TCA9537_ALL_PINS));          // Tous à 0
#endif
    
    // Initialiser le gestionnaire de capteurs (acquisition automatique)
    ESP_LOGI(TAG, "Initialisation Sensor Manager...");
    sensor_manager_config_t sensor_config = {
        .acquisition_period_ms = ADC_ACQUISITION_PERIOD_MS,
        .callback = sensor_acquisition_callback,
        .enable_logging = true  // Logs automatiques des acquisitions
    };
    ESP_ERROR_CHECK(sensor_manager_init(&sensor_config));
    
    // Initialiser ESP-NOW sécurisé
    ESP_LOGI(TAG, "Initialisation ESP-NOW sécurisé...");
    esp_now_secure_config_t espnow_config = {
        .node_id = NODE_ID,
        .channel = ESP_NOW_CHANNEL,
        .recv_cb = esp_now_recv_callback,
        .send_cb = esp_now_send_callback,
    };
    memcpy(espnow_config.aes_key, g_aes_key, 32);
    memcpy(espnow_config.hmac_key, g_hmac_key, 32);
    ESP_ERROR_CHECK(esp_now_secure_init(&espnow_config));
    
    // Initialiser Bluetooth SPP (scanner codes-barres)
#if ENABLE_BLUETOOTH_SPP
    ESP_LOGI(TAG, "Initialisation Bluetooth SPP...");
    bt_spp_config_t bt_config = {
        .device_name = BT_DEVICE_NAME,
        .pin_code = BT_PIN_CODE,
        .discoverable_at_init = BT_DISCOVERABLE_AT_INIT,
        .whitelist_addr = BT_WHITELIST_MAC,
        .data_cb = scanner_data_callback,
        .conn_cb = scanner_conn_callback
    };
    ESP_ERROR_CHECK(bt_spp_init(&bt_config));
#endif
    
    // Créer les tâches FreeRTOS
    ESP_LOGI(TAG, "Création des tâches...");
    
    // Heartbeat (priorité basse)
    xTaskCreate(heartbeat_task, "heartbeat", 2048, NULL, 1, NULL);
    
#if NODE_MODE == MODE_NODE
    // Tâche d'envoi ESP-NOW (uniquement en mode nœud)
    xTaskCreate(esp_now_tx_task, "esp_now_tx", 4096, NULL, 5, NULL);
#endif
    
    ESP_LOGI(TAG, "✅ Système initialisé et opérationnel");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📊 Configuration active:");
    ESP_LOGI(TAG, "  - ADS7128:    %s", ENABLE_ADS7128 ? "Actif" : "Inactif");
    ESP_LOGI(TAG, "  - ADS1119 #1: %s", ENABLE_ADS1119_1 ? "Actif" : "Inactif");
    ESP_LOGI(TAG, "  - ADS1119 #2: %s", ENABLE_ADS1119_2 ? "Actif" : "Inactif");
    ESP_LOGI(TAG, "  - TCA9537:    %s", ENABLE_TCA9537 ? "Actif" : "Inactif");
    ESP_LOGI(TAG, "  - Bluetooth:  %s", ENABLE_BLUETOOTH_SPP ? "Actif" : "Inactif");
    ESP_LOGI(TAG, "  - Simulation: %s", IS_SIMULATION ? "OUI" : "NON");
    
    // La boucle principale est gérée par FreeRTOS
    // Les acquisitions sont automatiques via sensor_manager
}
