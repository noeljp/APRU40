#ifndef ESP_NOW_SECURE_H
#define ESP_NOW_SECURE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_now.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Taille maximale des données utilisateur (250 - overhead sécurité) */
#define ESP_NOW_SECURE_MAX_DATA_LEN 200

/* Callback appelé quand un paquet sécurisé valide est reçu */
typedef void (*esp_now_secure_recv_cb_t)(const uint8_t *sender_mac, const uint8_t *data, uint8_t len, int8_t rssi);

/* Callback appelé quand un paquet est envoyé (succès/échec) */
typedef void (*esp_now_secure_send_cb_t)(const uint8_t *mac_addr, esp_now_send_status_t status);

/**
 * @brief Configuration ESP-NOW sécurisé
 */
typedef struct {
    uint8_t aes_key[32];         // Clé AES-256 (doit être unique par déploiement)
    uint8_t hmac_key[32];        // Clé HMAC-SHA256 (doit être différente de aes_key)
    uint8_t node_id;             // ID unique du nœud (1-255)
    uint8_t channel;             // Canal WiFi (1-13, 0=auto)
    esp_now_secure_recv_cb_t recv_cb;  // Callback réception
    esp_now_secure_send_cb_t send_cb;  // Callback émission
} esp_now_secure_config_t;

/**
 * @brief Initialiser ESP-NOW avec sécurité applicative
 * 
 * @param config Configuration avec clés et callbacks
 * @return ESP_OK si succès
 */
esp_err_t esp_now_secure_init(const esp_now_secure_config_t *config);

/**
 * @brief Envoyer des données sécurisées en broadcast
 * 
 * Les données sont chiffrées (AES-256-CBC), authentifiées (HMAC-SHA256)
 * et incluent un counter anti-replay.
 * 
 * @param data Données à envoyer
 * @param len Longueur des données (max ESP_NOW_SECURE_MAX_DATA_LEN)
 * @return ESP_OK si succès
 */
esp_err_t esp_now_secure_send(const uint8_t *data, uint8_t len);

/**
 * @brief Ajouter une adresse MAC à la whitelist (optionnel)
 * 
 * Si la whitelist est utilisée, seuls les nœuds autorisés seront acceptés.
 * Si aucune whitelist n'est configurée, tous les nœuds avec les bonnes clés sont acceptés.
 * 
 * @param mac_addr Adresse MAC du nœud autorisé
 * @return ESP_OK si succès
 */
esp_err_t esp_now_secure_add_trusted_peer(const uint8_t *mac_addr);

/**
 * @brief Obtenir l'adresse MAC locale
 * 
 * @param mac_addr Buffer pour stocker l'adresse MAC (6 octets)
 * @return ESP_OK si succès
 */
esp_err_t esp_now_secure_get_local_mac(uint8_t *mac_addr);

/**
 * @brief Obtenir les statistiques de sécurité
 * 
 * @param valid_packets Nombre de paquets valides reçus
 * @param invalid_hmac Nombre de paquets avec HMAC invalide
 * @param replay_attacks Nombre de replay attacks détectés
 * @param untrusted_peers Nombre de paquets de peers non autorisés
 */
void esp_now_secure_get_stats(uint32_t *valid_packets, uint32_t *invalid_hmac, 
                               uint32_t *replay_attacks, uint32_t *untrusted_peers);

/**
 * @brief Deinitialiser ESP-NOW sécurisé
 */
void esp_now_secure_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_NOW_SECURE_H */
