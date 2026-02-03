/**
 * @file config_manager.h
 * @brief Gestionnaire de configuration dynamique avec NVS et OTA
 * 
 * Permet de :
 * - Charger/sauvegarder la configuration en NVS (flash non-volatile)
 * - Modifier la configuration à distance via ESP-NOW
 * - Faire des mises à jour OTA du firmware
 * 
 * Architecture :
 * 1. Au boot : Charger config depuis NVS (ou utiliser défauts si première fois)
 * 2. Runtime : Modifier config via commandes ESP-NOW
 * 3. Sauvegarde : Persister automatiquement dans NVS
 * 4. OTA : Mettre à jour firmware complet si besoin
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * STRUCTURES DE CONFIGURATION
 * ============================================================================ */

/**
 * @brief Configuration du nœud (persistante en NVS)
 */
typedef struct {
    // Identité
    uint8_t node_id;                    // ID unique (1-254)
    char node_name[32];                 // Nom du nœud
    
    // Mode de fonctionnement
    uint8_t node_mode;                  // MODE_NODE(1) ou MODE_GATEWAY(2)
    
    // Périphériques activés
    bool enable_ads7128;
    bool enable_ads1119_1;
    bool enable_ads1119_2;
    bool enable_tca9537;
    bool enable_bluetooth;
    
    // Périodes (millisecondes)
    uint32_t adc_period_ms;             // Période acquisition ADC
    uint32_t tx_period_ms;              // Période transmission ESP-NOW
    uint32_t heartbeat_period_ms;       // Période heartbeat
    
    // ESP-NOW
    uint8_t espnow_channel;             // Canal WiFi (0=auto)
    uint8_t aes_key[32];                // Clé AES-256
    uint8_t hmac_key[32];               // Clé HMAC-256
    
    // Bluetooth
    char bt_device_name[32];
    char bt_pin_code[16];
    bool bt_discoverable;
    uint8_t bt_whitelist_mac[6];        // 00:00:00:00:00:00 = tous autorisés
    
    // Métadonnées
    uint32_t version;                   // Version de config (incrémenté à chaque modif)
    uint32_t crc32;                     // CRC32 pour vérifier intégrité
    
} node_config_runtime_t;

/**
 * @brief Statistiques de configuration
 */
typedef struct {
    uint32_t load_count;                // Nombre de chargements depuis NVS
    uint32_t save_count;                // Nombre de sauvegardes
    uint32_t update_count;              // Nombre de mises à jour reçues
    uint32_t ota_count;                 // Nombre d'OTA réussies
    uint32_t last_update_timestamp;     // Timestamp dernière modification
} config_stats_t;

/* ============================================================================
 * COMMANDES DE CONFIGURATION À DISTANCE
 * ============================================================================ */

/**
 * @brief Types de commandes de configuration
 */
typedef enum {
    CONFIG_CMD_SET_NODE_ID = 0x01,          // Changer node_id
    CONFIG_CMD_SET_NODE_NAME = 0x02,        // Changer nom
    CONFIG_CMD_SET_MODE = 0x03,             // Changer mode nœud/gateway
    CONFIG_CMD_ENABLE_DEVICE = 0x04,        // Activer/désactiver périphérique
    CONFIG_CMD_SET_PERIOD = 0x05,           // Modifier période
    CONFIG_CMD_SET_ESPNOW_CHANNEL = 0x06,   // Changer canal WiFi
    CONFIG_CMD_SET_BT_CONFIG = 0x07,        // Config Bluetooth
    CONFIG_CMD_RELOAD = 0x10,               // Recharger config depuis NVS
    CONFIG_CMD_SAVE = 0x11,                 // Sauvegarder en NVS
    CONFIG_CMD_RESET_TO_DEFAULTS = 0x12,    // Restaurer config par défaut
    CONFIG_CMD_GET_CONFIG = 0x20,           // Obtenir config actuelle (réponse JSON)
    CONFIG_CMD_OTA_START = 0x30,            // Démarrer OTA
    CONFIG_CMD_REBOOT = 0xFF,               // Redémarrer module
} config_cmd_type_t;

/**
 * @brief Paquet de commande de configuration (envoyé via ESP-NOW)
 */
typedef struct __attribute__((packed)) {
    uint8_t cmd_type;                   // Type de commande
    uint8_t target_node_id;             // ID du nœud cible (0xFF = broadcast)
    uint16_t data_len;                  // Longueur des données
    uint8_t data[200];                  // Données de la commande
} config_cmd_packet_t;

/* ============================================================================
 * API PUBLIQUE
 * ============================================================================ */

/**
 * @brief Initialiser le gestionnaire de configuration
 * 
 * Charge la configuration depuis NVS, ou utilise les valeurs par défaut
 * de node_config.h si première initialisation.
 * 
 * @return ESP_OK si succès
 */
esp_err_t config_manager_init(void);

/**
 * @brief Obtenir la configuration actuelle
 * 
 * @param[out] config Pointeur où copier la config (thread-safe)
 * @return ESP_OK si succès
 */
esp_err_t config_manager_get(node_config_runtime_t *config);

/**
 * @brief Modifier la configuration
 * 
 * @param config Nouvelle configuration
 * @param auto_save Si true, sauvegarde automatiquement en NVS
 * @return ESP_OK si succès
 */
esp_err_t config_manager_set(const node_config_runtime_t *config, bool auto_save);

/**
 * @brief Sauvegarder la configuration en NVS
 * 
 * Persiste la config actuelle dans la flash non-volatile.
 * 
 * @return ESP_OK si succès
 */
esp_err_t config_manager_save(void);

/**
 * @brief Recharger la configuration depuis NVS
 * 
 * @return ESP_OK si succès
 */
esp_err_t config_manager_reload(void);

/**
 * @brief Restaurer la configuration par défaut (node_config.h)
 * 
 * @param auto_save Si true, sauvegarde en NVS immédiatement
 * @return ESP_OK si succès
 */
esp_err_t config_manager_reset_to_defaults(bool auto_save);

/**
 * @brief Traiter une commande de configuration reçue
 * 
 * Appelé automatiquement depuis le callback ESP-NOW quand un paquet
 * de commande est détecté.
 * 
 * @param packet Paquet de commande
 * @return ESP_OK si commande traitée avec succès
 */
esp_err_t config_manager_process_command(const config_cmd_packet_t *packet);

/**
 * @brief Obtenir statistiques de configuration
 * 
 * @param[out] stats Pointeur où copier les stats
 * @return ESP_OK si succès
 */
esp_err_t config_manager_get_stats(config_stats_t *stats);

/**
 * @brief Formater la configuration en JSON
 * 
 * @param[out] json_buffer Buffer où écrire le JSON
 * @param buffer_size Taille du buffer
 * @return Nombre de caractères écrits, ou -1 si erreur
 */
int config_manager_format_json(char *json_buffer, size_t buffer_size);

/**
 * @brief Envoyer la configuration actuelle via ESP-NOW (pour debug)
 * 
 * @param dest_mac Adresse MAC destination (NULL = broadcast)
 * @return ESP_OK si succès
 */
esp_err_t config_manager_send_config(const uint8_t *dest_mac);

/* ============================================================================
 * OTA (Over-The-Air firmware update)
 * ============================================================================ */

/**
 * @brief Démarrer une mise à jour OTA du firmware
 * 
 * @param url URL du firmware (.bin) sur serveur HTTP/HTTPS
 * @return ESP_OK si OTA démarre (asynchrone)
 */
esp_err_t config_manager_ota_start(const char *url);

/**
 * @brief Obtenir le statut de l'OTA en cours
 * 
 * @param[out] progress Progression (0-100%)
 * @param[out] status_msg Message de statut
 * @return true si OTA en cours
 */
bool config_manager_ota_get_status(uint8_t *progress, const char **status_msg);

/* ============================================================================
 * HELPERS
 * ============================================================================ */

/**
 * @brief Créer un paquet de commande de configuration
 * 
 * Exemple : Changer le node_id à 5
 * 
 * @code
 * config_cmd_packet_t cmd;
 * uint8_t new_id = 5;
 * config_manager_create_command(&cmd, CONFIG_CMD_SET_NODE_ID, 3, &new_id, 1);
 * esp_now_secure_send((uint8_t*)&cmd, sizeof(cmd));
 * @endcode
 * 
 * @param[out] packet Paquet à remplir
 * @param cmd_type Type de commande
 * @param target_node_id ID du nœud cible (0xFF = tous)
 * @param data Données de la commande
 * @param data_len Longueur des données
 * @return ESP_OK si succès
 */
esp_err_t config_manager_create_command(config_cmd_packet_t *packet,
                                        config_cmd_type_t cmd_type,
                                        uint8_t target_node_id,
                                        const uint8_t *data,
                                        uint16_t data_len);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
