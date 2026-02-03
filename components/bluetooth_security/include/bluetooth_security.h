/**
 * @file bluetooth_security.h
 * @brief Surcouche de sécurité pour bluetooth_spp.h
 * 
 * Fonctionnalités additionnelles :
 * - PIN aléatoire généré au boot (stocké NVS)
 * - Whitelist MAC stricte (UN SEUL scanner autorisé)
 * - Alertes sur tentatives connexion non autorisées
 * - Génération et affichage PIN pour opérateur (LED/logs)
 * 
 * Architecture : Un nœud = Un scanner Zebra (relation 1:1)
 * 
 * Conformité NIS2 : Article 21 - Contrôle d'accès + Chiffrement
 */

#ifndef BLUETOOTH_SECURITY_H
#define BLUETOOTH_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "bluetooth_spp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BT_SEC_PIN_LENGTH   6   // PIN 6 chiffres (ex: "123456")

/**
 * @brief Callback appelé lors de tentative connexion non autorisée
 * 
 * @param remote_mac Adresse MAC du device refusé
 */
typedef void (*bt_security_unauthorized_callback_t)(const uint8_t *remote_mac);

/**
 * @brief Configuration sécurité Bluetooth
 */
typedef struct {
    const char *device_name;                            // Nom Bluetooth (ex: "APRU40-Node-01")
    const uint8_t *scanner_mac;                         // MAC scanner Zebra autorisé (6 octets)
    bool generate_random_pin;                           // true = PIN aléatoire au boot, false = PIN fourni
    const char *fixed_pin;                              // PIN fixe (si generate_random_pin=false)
    bool store_pin_nvs;                                 // true = stocker PIN en NVS (persistant)
    bt_spp_data_callback_t data_cb;                     // Callback réception données scanner
    bt_security_unauthorized_callback_t unauthorized_cb; // Callback tentative connexion non autorisée
} bt_security_config_t;

/**
 * @brief Initialiser Bluetooth avec sécurité renforcée
 * 
 * Génère PIN aléatoire (si demandé), configure whitelist MAC stricte,
 * initialise bluetooth_spp avec callbacks de sécurité.
 * 
 * @param config Configuration sécurité
 * @return ESP_OK si succès
 * 
 * @note Le PIN généré est affiché dans les logs et peut être récupéré
 *       avec bt_security_get_pin() pour affichage LED/écran.
 */
esp_err_t bt_security_init(const bt_security_config_t *config);

/**
 * @brief Obtenir le PIN actuel (pour affichage opérateur)
 * 
 * @param pin_out Buffer pour stocker le PIN (min BT_SEC_PIN_LENGTH+1 octets)
 * @return ESP_OK si succès
 */
esp_err_t bt_security_get_pin(char *pin_out);

/**
 * @brief Vérifier si un device MAC est autorisé
 * 
 * @param remote_mac Adresse MAC à vérifier (6 octets)
 * @return true si autorisé (correspond au scanner configuré)
 */
bool bt_security_is_authorized(const uint8_t *remote_mac);

/**
 * @brief Obtenir la MAC du scanner autorisé
 * 
 * @param mac_out Buffer pour stocker la MAC (6 octets)
 * @return ESP_OK si succès
 */
esp_err_t bt_security_get_scanner_mac(uint8_t *mac_out);

/**
 * @brief Changer le scanner autorisé (reconfiguration whitelist)
 * 
 * Permet de changer le scanner Zebra en cas de remplacement hardware.
 * Nécessite redémarrage Bluetooth pour appliquer.
 * 
 * @param new_mac Nouvelle MAC scanner (6 octets)
 * @param store_nvs true = stocker en NVS (persistant)
 * @return ESP_OK si succès
 */
esp_err_t bt_security_update_scanner_mac(const uint8_t *new_mac, bool store_nvs);

/**
 * @brief Générer un nouveau PIN aléatoire
 * 
 * Utile pour rotation périodique du PIN (recommandé tous les 6 mois).
 * Nécessite redémarrage Bluetooth pour appliquer.
 * 
 * @param store_nvs true = stocker en NVS (persistant)
 * @return ESP_OK si succès
 */
esp_err_t bt_security_regenerate_pin(bool store_nvs);

/**
 * @brief Activer/désactiver mode discoverable temporairement
 * 
 * Utile pour pairing initial (5 min fenêtre) puis désactivation automatique.
 * 
 * @param enable true = discoverable, false = caché
 * @param timeout_sec Durée avant désactivation auto (0 = pas de timeout)
 * @return ESP_OK si succès
 */
esp_err_t bt_security_set_discoverable(bool enable, uint32_t timeout_sec);

/**
 * @brief Obtenir statistiques sécurité Bluetooth
 * 
 * @param total_connections_out Nombre total de connexions acceptées
 * @param rejected_connections_out Nombre de connexions refusées (whitelist)
 * @return ESP_OK si succès
 */
esp_err_t bt_security_get_stats(uint32_t *total_connections_out, uint32_t *rejected_connections_out);

/**
 * @brief Désinstaller Bluetooth security
 */
void bt_security_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_SECURITY_H */
