/**
 * @file tamper_security.h
 * @brief Gestion du tamper switch pour sécurité physique
 * 
 * Détection d'ouverture du boîtier avec réaction automatique :
 * - Effacement des clés NVS (certificats, configuration sensible)
 * - Envoi d'alerte MQTT (si gateway)
 * - Envoi d'alerte ESP-NOW (si nœud)
 * - Redémarrage sécurisé
 * 
 * Conformité NIS2 : Article 21 - Gestion des risques physiques
 */

#ifndef TAMPER_SECURITY_H
#define TAMPER_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration du tamper security
 */
typedef struct {
    gpio_num_t tamper_gpio;                 // GPIO connecté au switch tamper (ex: GPIO_NUM_34)
    bool active_low;                        // true = déclenchement sur LOW (switch fermé normalement)
    bool auto_erase_nvs;                    // true = effacer NVS automatiquement
    bool auto_restart;                      // true = redémarrer après détection
    uint32_t debounce_ms;                   // Durée anti-rebond (recommandé: 50-100ms)
    void (*alert_callback)(void);           // Callback appelé avant effacement (pour alerte MQTT/ESP-NOW)
} tamper_security_config_t;

/**
 * @brief Initialiser la détection tamper
 * 
 * Configure le GPIO en entrée avec interruption sur front montant/descendant.
 * L'ISR déclenche l'effacement NVS et l'alerte si configuré.
 * 
 * @param config Configuration tamper (GPIO, comportement)
 * @return ESP_OK si succès
 * 
 * @note ⚠️ CRITIQUE : Cette fonction DOIT être appelée AVANT tout autre composant
 *       pour garantir la protection des clés en cas d'ouverture au boot.
 */
esp_err_t tamper_security_init(const tamper_security_config_t *config);

/**
 * @brief Vérifier si le tamper a été déclenché
 * 
 * @return true si tamper détecté (état persistant jusqu'au restart)
 */
bool tamper_security_is_triggered(void);

/**
 * @brief Obtenir le nombre de déclenchements tamper depuis dernier reset
 * 
 * @return Nombre de déclenchements (0 si aucun)
 */
uint32_t tamper_security_get_trigger_count(void);

/**
 * @brief Test manuel du tamper (pour validation déploiement)
 * 
 * Simule un déclenchement tamper pour tester la réaction (effacement NVS, alerte).
 * 
 * @warning ⚠️ Cette fonction EFFACE LES CLÉS si auto_erase_nvs=true !
 *          À utiliser UNIQUEMENT en développement ou avec ESP32 de test.
 * 
 * @return ESP_OK si test réussi
 */
esp_err_t tamper_security_test(void);

/**
 * @brief Désactiver temporairement le tamper (maintenance)
 * 
 * Désactive l'interruption GPIO pour permettre ouverture sans effacement.
 * DOIT être ré-activé avec tamper_security_enable() après maintenance.
 * 
 * @note 🔐 Nécessite authentification préalable (à implémenter dans application)
 * 
 * @return ESP_OK si succès
 */
esp_err_t tamper_security_disable(void);

/**
 * @brief Ré-activer le tamper après maintenance
 * 
 * @return ESP_OK si succès
 */
esp_err_t tamper_security_enable(void);

/**
 * @brief Désinstaller le tamper security
 * 
 * Libère les ressources (interruption GPIO, mémoire).
 */
void tamper_security_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* TAMPER_SECURITY_H */
