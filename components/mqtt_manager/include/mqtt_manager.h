/**
 * @file mqtt_manager.h
 * @brief Gestionnaire MQTT pour passerelles APRU40
 * 
 * Gère la connexion au broker Mosquitto avec TLS 1.3 et la publication/réception
 * des données et configurations.
 * 
 * Architecture :
 * - Connexion sécurisée MQTT/TLS 1.3 (mTLS)
 * - Publication données nœuds vers cloud
 * - Réception commandes/configurations
 * - Gestion OTA firmware via MQTT
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include "mqtt_client.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TYPES ET STRUCTURES
 * ============================================================================ */

/**
 * @brief Callback pour réception de messages MQTT
 * 
 * @param topic Topic MQTT reçu
 * @param data Payload du message
 * @param len Longueur du payload
 */
typedef void (*mqtt_message_callback_t)(const char *topic, const char *data, size_t len);

/**
 * @brief Callback pour événements de connexion MQTT
 * 
 * @param connected true si connecté, false si déconnecté
 */
typedef void (*mqtt_connection_callback_t)(bool connected);

/**
 * @brief Configuration MQTT manager
 */
typedef struct {
    const char *broker_uri;         ///< URI broker (mqtts://...)
    const char *client_id;          ///< ID client unique
    const char *username;           ///< Username (optionnel)
    const char *password;           ///< Password (optionnel)
    
    // Certificats TLS
    const char *ca_cert_pem;        ///< Certificat CA (PEM)
    const char *client_cert_pem;    ///< Certificat client (PEM)
    const char *client_key_pem;     ///< Clé privée client (PEM)
    
    // Callbacks
    mqtt_message_callback_t msg_cb; ///< Callback réception messages
    mqtt_connection_callback_t conn_cb; ///< Callback événements connexion
    
    // Paramètres
    uint16_t keepalive_sec;         ///< Keepalive (défaut 60s)
    uint8_t qos;                    ///< QoS (0, 1 ou 2)
    bool retain;                    ///< Retain flag
} mqtt_manager_config_t;

/**
 * @brief Statistiques MQTT
 */
typedef struct {
    uint32_t messages_published;    ///< Nombre de messages publiés
    uint32_t messages_received;     ///< Nombre de messages reçus
    uint32_t publish_errors;        ///< Erreurs de publication
    uint32_t reconnections;         ///< Nombre de reconnexions
    bool is_connected;              ///< État de connexion actuel
    int32_t last_error;             ///< Dernier code d'erreur
} mqtt_manager_stats_t;

/* ============================================================================
 * API PUBLIQUE
 * ============================================================================ */

/**
 * @brief Initialiser le gestionnaire MQTT
 * 
 * @param config Configuration MQTT
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_init(const mqtt_manager_config_t *config);

/**
 * @brief Démarrer la connexion MQTT
 * 
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_start(void);

/**
 * @brief Arrêter la connexion MQTT
 * 
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_stop(void);

/**
 * @brief S'abonner à un topic MQTT
 * 
 * @param topic Topic à s'abonner (peut contenir wildcards +, #)
 * @param qos QoS (0, 1 ou 2)
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_subscribe(const char *topic, uint8_t qos);

/**
 * @brief Se désabonner d'un topic MQTT
 * 
 * @param topic Topic à désabonner
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_unsubscribe(const char *topic);

/**
 * @brief Publier un message MQTT
 * 
 * @param topic Topic de publication
 * @param data Payload (peut être binaire)
 * @param len Longueur du payload
 * @param qos QoS (0, 1 ou 2)
 * @param retain Retain flag
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_publish(const char *topic, const char *data, size_t len, 
                               uint8_t qos, bool retain);

/**
 * @brief Publier un message JSON formaté
 * 
 * @param topic Topic de publication
 * @param json_str Chaîne JSON (null-terminated)
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_publish_json(const char *topic, const char *json_str);

/**
 * @brief Publier heartbeat/status de la gateway
 * 
 * Publie automatiquement sur topic apru40/gateway/{client_id}/status
 * Format JSON : {"status":"online", "timestamp":..., "uptime":...}
 * 
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_publish_heartbeat(void);

/**
 * @brief Obtenir les statistiques MQTT
 * 
 * @param stats Pointeur vers structure stats à remplir
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_get_stats(mqtt_manager_stats_t *stats);

/**
 * @brief Vérifier si connecté au broker
 * 
 * @return true si connecté, false sinon
 */
bool mqtt_manager_is_connected(void);

/**
 * @brief Charger certificats TLS depuis carte SD et stocker en NVS chiffré
 * 
 * Workflow de provisioning sécurisé :
 * 1. Insérer carte SD avec /certs/ca.crt, client.crt, client.key
 * 2. Démarrer ESP32 : cette fonction charge et stocke en NVS chiffré
 * 3. Retirer carte SD : certificats restent en NVS sécurisé
 * 
 * @param mount_point Point de montage SD (ex: "/sdcard")
 * @return ESP_OK si succès, code erreur sinon
 * 
 * @note Les certificats en NVS sont automatiquement chiffrés si CONFIG_NVS_ENCRYPTION=y
 */
esp_err_t mqtt_manager_load_certs_from_sd(const char *mount_point);

/**
 * @brief Charger certificats TLS depuis NVS (après provisioning SD)
 * 
 * Charge les certificats précédemment stockés par mqtt_manager_load_certs_from_sd().
 * Utilisé au boot normal (sans carte SD).
 * 
 * @param ca_out Buffer pour CA (alloué par cette fonction)
 * @param cert_out Buffer pour certificat (alloué par cette fonction)
 * @param key_out Buffer pour clé (alloué par cette fonction)
 * @return ESP_OK si succès, ESP_ERR_NVS_NOT_FOUND si pas de certificats
 * 
 * @note Les buffers doivent être libérés avec free() après usage
 */
esp_err_t mqtt_manager_load_certs_from_nvs(char **ca_out,
                                           char **cert_out,
                                           char **key_out);

/**
 * @brief Charger certificats TLS (méthode automatique)
 * 
 * Tente d'abord NVS, puis carte SD si NVS vide.
 * Méthode recommandée pour app_main().
 * 
 * @param ca_out Buffer pour CA (alloué par cette fonction)
 * @param cert_out Buffer pour certificat (alloué par cette fonction)  
 * @param key_out Buffer pour clé (alloué par cette fonction)
 * @return ESP_OK si succès, code erreur sinon
 * 
 * @note Les buffers doivent être libérés avec free() après usage
 */
esp_err_t mqtt_manager_load_certs(char **ca_out,
                                  char **cert_out,
                                  char **key_out);

/**
 * @brief Effacer certificats du NVS
 * 
 * Utile pour réinitialiser ou forcer un nouveau provisioning.
 * 
 * @return ESP_OK si succès, code erreur sinon
 */
esp_err_t mqtt_manager_clear_certs(void);

#ifdef __cplusplus
}
#endif

#endif // MQTT_MANAGER_H
