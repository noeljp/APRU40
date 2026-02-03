#ifndef BLUETOOTH_SPP_H
#define BLUETOOTH_SPP_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BT_SPP_MAX_LINE_LEN 256

/* Callback appelé quand une ligne complète est reçue (terminée par \n) */
typedef void (*bt_spp_data_callback_t)(const char *line, size_t len);

/* Callback appelé lors de connexion/déconnexion */
typedef void (*bt_spp_conn_callback_t)(bool connected, const uint8_t *remote_addr);

/**
 * @brief Configuration Bluetooth SPP Server
 */
typedef struct {
    const char *device_name;           // Nom Bluetooth (ex: "TAS-MACHINE-QR-01")
    const char *pin_code;              // PIN fixe 6+ chiffres (ex: "736281")
    bool discoverable_at_init;         // true = visible au démarrage pour pairing
    const uint8_t *whitelist_addr;     // Adresse BT autorisée (6 octets, NULL = tous)
    bt_spp_data_callback_t data_cb;    // Callback réception ligne
    bt_spp_conn_callback_t conn_cb;    // Callback connexion/déconnexion
} bt_spp_config_t;

/**
 * @brief Initialiser Bluetooth SPP Server
 * 
 * @param config Configuration avec nom, PIN, callbacks
 * @return ESP_OK si succès
 */
esp_err_t bt_spp_init(const bt_spp_config_t *config);

/**
 * @brief Activer/désactiver le mode discoverable
 * 
 * @param discoverable true = visible en scan Bluetooth, false = caché
 * @return ESP_OK si succès
 */
esp_err_t bt_spp_set_discoverable(bool discoverable);

/**
 * @brief Envoyer des données au client SPP connecté
 * 
 * @param data Données à envoyer
 * @param len Longueur des données
 * @return ESP_OK si succès
 */
esp_err_t bt_spp_send(const uint8_t *data, size_t len);

/**
 * @brief Obtenir l'adresse Bluetooth locale (MAC)
 * 
 * @param addr Buffer pour stocker l'adresse (6 octets)
 * @return ESP_OK si succès
 */
esp_err_t bt_spp_get_local_addr(uint8_t *addr);

/**
 * @brief Déconnecter le client SPP actuel
 * 
 * @return ESP_OK si succès
 */
esp_err_t bt_spp_disconnect(void);

/**
 * @brief Désinitialer Bluetooth SPP
 */
void bt_spp_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_SPP_H */
