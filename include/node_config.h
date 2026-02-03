/**
 * @file node_config.h
 * @brief Configuration centralisée du nœud (mode, périphériques, réseau)
 * 
 * Ce fichier permet de configurer chaque module en nœud capteur ou gateway
 * sans modifier le code principal. Simplement changer les #define et recompiler.
 */

#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * MODE DE FONCTIONNEMENT
 * ============================================================================ */

/**
 * Définir le mode du module : 
 * - MODE_NODE : Nœud capteur (acquisition + envoi ESP-NOW + Bluetooth pour scanner Zebra)
 * - MODE_GATEWAY : Gateway (réception ESP-NOW + Ethernet/MQTT vers Mosquitto)
 */
#define MODE_NODE       1
#define MODE_GATEWAY    2

// ⚠️ À MODIFIER POUR CHAQUE MODULE
#define NODE_MODE       MODE_NODE

/* ============================================================================
 * IDENTITÉ DU NŒUD
 * ============================================================================ */

// ID unique du nœud (1-254, 255 réservé pour broadcast)
// ⚠️ À MODIFIER POUR CHAQUE MODULE
#define NODE_ID         1

// Nom du nœud (pour logs et Bluetooth)
#define NODE_NAME       "APRU40-Node-01"

/* ============================================================================
 * CONFIGURATION PÉRIPHÉRIQUES I2C
 * ============================================================================ */

// Activer/désactiver chaque périphérique
#define ENABLE_ADS7128      1   // ADC 12-bit 8 canaux (0x17)
#define ENABLE_ADS1119_1    1   // ADC 16-bit 4 canaux #1 (0x40)
#define ENABLE_ADS1119_2    1   // ADC 16-bit 4 canaux #2 (0x41)
#define ENABLE_TCA9537      1   // GPIO expander 4-bit (0x49)

// Adresses I2C
#define ADS7128_I2C_ADDR    0x17
#define ADS1119_1_I2C_ADDR  0x40
#define ADS1119_2_I2C_ADDR  0x41
#define TCA9537_I2C_ADDR    0x49

/* ============================================================================
 * PÉRIODES D'ACQUISITION (ms) - MODE_NODE
 * ============================================================================ */

#define ADC_ACQUISITION_PERIOD_MS   5000    // 5 secondes (acquisition capteurs)
#define GPIO_CONTROL_PERIOD_MS      1000    // 1 seconde (contrôle GPIO)
#define HEARTBEAT_PERIOD_MS         1000    // 1 seconde (LED heartbeat)

/* ============================================================================
 * CONFIGURATION ESP-NOW
 * ============================================================================ */

// Canal WiFi (0=auto, 1-13 manuel)
#define ESP_NOW_CHANNEL             0

// Période d'envoi des données (ms) - uniquement pour MODE_NODE
#define ESP_NOW_TX_PERIOD_MS        10000   // 10 secondes

// Clés de sécurité (⚠️ À MODIFIER EN PRODUCTION - voir README)
// Générer avec : openssl rand -hex 32
#define AES_KEY { \
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, \
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c, \
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, \
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c \
}

#define HMAC_KEY { \
    0xc0, 0x9f, 0xbb, 0xe9, 0x2e, 0x4f, 0xa5, 0x23, \
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, \
    0xc0, 0x9f, 0xbb, 0xe9, 0x2e, 0x4f, 0xa5, 0x23, \
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 \
}

/* ============================================================================
 * CONFIGURATION BLUETOOTH - MODE_NODE UNIQUEMENT
 * ============================================================================ */

/**
 * Bluetooth SPP : Uniquement pour nœuds équipés d'un scanner Zebra DS2278
 * - Réception des codes QR/codes-barres scannés
 * - Transmission avec données capteurs via ESP-NOW
 * - Désactiver sur passerelles (pas utilisé)
 */

// Activer/désactiver Bluetooth scanner (1 = avec scanner Zebra, 0 = sans)
#define ENABLE_BLUETOOTH_SPP        1

// Configuration Bluetooth
#define BT_DEVICE_NAME              NODE_NAME
#define BT_PIN_CODE                 "1234"
#define BT_DISCOVERABLE_AT_INIT     true    // true pendant pairing initial

// Configuration scanner Zebra DS2278
#define BT_SCANNER_BAUDRATE         9600
#define BT_SCANNER_SUFFIX           "\r\n"  // CR+LF

// ⚠️ SÉCURITÉ : Whitelist stricte - UN SEUL scanner autorisé par nœud
// Format : {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
// À configurer avec la MAC du scanner Zebra DS2278 spécifique
// Obtenir MAC : Scanner DS2278 > Settings > About > Bluetooth Address
#define BT_SCANNER_MAC_WHITELIST    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // ⚠️ À REMPLACER

/* ============================================================================
 * SÉCURITÉ PHYSIQUE - TAMPER SWITCH
 * ============================================================================ */

// GPIO connecté au switch tamper (détection ouverture boîtier)
// Recommandé : GPIO34-39 (input-only, pas besoin de pull-up externe)
#define TAMPER_GPIO                 GPIO_NUM_34

// Active low : true = switch fermé normalement (LOW), ouverture = HIGH
// Active high : false = switch ouvert normalement (HIGH), fermeture = LOW
#define TAMPER_ACTIVE_LOW           true

// ⚠️ MODE DÉVELOPPEMENT : Effacement et redémarrage DÉSACTIVÉS (pas de boîtier)
// Effacement automatique NVS si tamper détecté (PRODUCTION: true, DEBUG: false)
#define TAMPER_AUTO_ERASE_NVS       false   // ⚠️ DEV: Désactivé

// Redémarrage automatique après tamper (PRODUCTION: true, DEBUG: false)
#define TAMPER_AUTO_RESTART         false   // ⚠️ DEV: Désactivé

// Durée anti-rebond switch (ms)
#define TAMPER_DEBOUNCE_MS          100

/* ============================================================================
 * CONFIGURATION GATEWAY (si NODE_MODE == MODE_GATEWAY)
 * ============================================================================ */

#if NODE_MODE == MODE_GATEWAY

/**
 * Architecture Gateway :
 * - Réception données nœuds via ESP-NOW (chiffré AES + HMAC)
 * - Publication vers broker Mosquitto via Ethernet + MQTT/TLS 1.3
 * - Réception commandes/configurations via MQTT
 * - PAS de Bluetooth (uniquement sur nœuds avec scanner)
 */

// Interface Ethernet ESP32-POE-ISO
#define ENABLE_ETHERNET             1
#define ETH_PHY_ADDR                0
#define ETH_PHY_RST_GPIO            -1      // -1 = pas de GPIO reset
#define ETH_MDC_GPIO                23
#define ETH_MDIO_GPIO               18

// MQTT Broker Mosquitto avec TLS 1.3
#define MQTT_BROKER_URI             "mqtts://broker.example.com:8883"  // mqtts = TLS
#define MQTT_CLIENT_ID              "APRU40_GW001"
#define MQTT_USERNAME               "apru40_gateway"
#define MQTT_PASSWORD               "***"  // ⚠️ À stocker en NVS en production

// Topics MQTT
#define MQTT_TOPIC_DATA             "apru40/gateway/" MQTT_CLIENT_ID "/data"
#define MQTT_TOPIC_STATUS           "apru40/gateway/" MQTT_CLIENT_ID "/status"
#define MQTT_TOPIC_CMD              "apru40/gateway/" MQTT_CLIENT_ID "/cmd"
#define MQTT_TOPIC_CONFIG           "apru40/gateway/" MQTT_CLIENT_ID "/config"
#define MQTT_TOPIC_NODE_CONFIG      "apru40/node/+/config"     // + = wildcard node_id
#define MQTT_TOPIC_NODE_OTA         "apru40/node/+/ota"

// Certificats TLS (à stocker dans SPIFFS)
// ⚠️ En production : charger depuis /spiffs/certs/
#define MQTT_CA_CERT_PATH           "/spiffs/ca.crt"
#define MQTT_CLIENT_CERT_PATH       "/spiffs/client.crt"
#define MQTT_CLIENT_KEY_PATH        "/spiffs/client.key"

// Configuration TLS
#define MQTT_TLS_VERSION            "TLSv1.3"
#define MQTT_VERIFY_PEER            true    // Vérification certificat serveur
#define MQTT_MUTUAL_AUTH            true    // Authentification mutuelle (mTLS)

// Paramètres MQTT
#define MQTT_KEEPALIVE_SEC          60
#define MQTT_QOS                    1       // QoS 1 = Au moins une fois
#define MQTT_RETAIN                 false
#define MQTT_RECONNECT_TIMEOUT_MS   5000

// Buffer données nœuds
#define MAX_NODES                   30      // Jusqu'à 30 nœuds
#define NODE_DATA_BUFFER_SIZE       100     // 100 échantillons par nœud

#endif // MODE_GATEWAY

/* ============================================================================
 * MODE SIMULATION
 * ============================================================================ */

// Mode simulation (aucun I2C réel) - défini dans platformio.ini
#ifdef SIMULATION_MODE
#define IS_SIMULATION   1
#else
#define IS_SIMULATION   0
#endif

#endif // NODE_CONFIG_H
