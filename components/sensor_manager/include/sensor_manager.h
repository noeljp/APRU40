/**
 * @file sensor_manager.h
 * @brief Gestionnaire unifié de tous les capteurs ADC
 * 
 * Ce composant centralise l'acquisition de tous les ADC (ADS7128, ADS1119)
 * avec conversions physiques automatiques et gestion des tâches en arrière-plan.
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TYPES ET STRUCTURES
 * ============================================================================ */

/**
 * @brief Type de capteur ADC
 */
typedef enum {
    SENSOR_TYPE_ADS7128 = 0,
    SENSOR_TYPE_ADS1119_1,
    SENSOR_TYPE_ADS1119_2,
    SENSOR_TYPE_COUNT
} sensor_type_t;

/**
 * @brief Donnée brute d'un canal
 */
typedef struct {
    uint32_t timestamp_ms;      // Timestamp acquisition (ms depuis boot)
    int32_t raw_value;          // Valeur brute ADC
    float physical_value;       // Valeur physique convertie
    bool valid;                 // true si donnée valide
} sensor_data_t;

/**
 * @brief Structure complète des données d'un capteur
 */
typedef struct {
    sensor_type_t type;
    uint8_t num_channels;
    sensor_data_t channels[8];  // Max 8 canaux (ADS7128)
    uint32_t last_update_ms;
    uint32_t error_count;
} sensor_readings_t;

/**
 * @brief Callback appelé après chaque acquisition réussie
 * 
 * @param type Type de capteur
 * @param readings Pointeur vers les données lues
 */
typedef void (*sensor_callback_t)(sensor_type_t type, const sensor_readings_t *readings);

/**
 * @brief Configuration du sensor manager
 */
typedef struct {
    uint32_t acquisition_period_ms;     // Période d'acquisition (ms)
    sensor_callback_t callback;         // Callback optionnel après acquisition
    bool enable_logging;                // Activer logs console automatiques
} sensor_manager_config_t;

/* ============================================================================
 * API PUBLIQUE
 * ============================================================================ */

/**
 * @brief Initialiser le gestionnaire de capteurs
 * 
 * Initialise tous les capteurs configurés dans node_config.h et démarre
 * les tâches d'acquisition en arrière-plan.
 * 
 * @param config Configuration du gestionnaire
 * @return ESP_OK si succès, code d'erreur sinon
 */
esp_err_t sensor_manager_init(const sensor_manager_config_t *config);

/**
 * @brief Obtenir les dernières données d'un capteur
 * 
 * Retourne une copie des dernières données acquises (thread-safe).
 * 
 * @param type Type de capteur
 * @param[out] readings Pointeur où copier les données
 * @return ESP_OK si succès, ESP_ERR_INVALID_ARG si capteur inexistant
 */
esp_err_t sensor_manager_get_readings(sensor_type_t type, sensor_readings_t *readings);

/**
 * @brief Obtenir la valeur physique d'un canal spécifique
 * 
 * @param type Type de capteur
 * @param channel Numéro de canal
 * @param[out] value Pointeur où écrire la valeur physique
 * @return ESP_OK si succès
 */
esp_err_t sensor_manager_get_channel_value(sensor_type_t type, uint8_t channel, float *value);

/**
 * @brief Obtenir le nom et l'unité d'un canal
 * 
 * @param type Type de capteur
 * @param channel Numéro de canal
 * @param[out] name Pointeur vers le nom (ne pas libérer)
 * @param[out] unit Pointeur vers l'unité (ne pas libérer)
 * @return ESP_OK si succès
 */
esp_err_t sensor_manager_get_channel_info(sensor_type_t type, uint8_t channel, 
                                           const char **name, const char **unit);

/**
 * @brief Forcer une acquisition immédiate (synchrone)
 * 
 * Bloque jusqu'à ce que l'acquisition soit complétée.
 * 
 * @param type Type de capteur
 * @return ESP_OK si succès
 */
esp_err_t sensor_manager_trigger_acquisition(sensor_type_t type);

/**
 * @brief Obtenir les statistiques d'un capteur
 * 
 * @param type Type de capteur
 * @param[out] total_acquisitions Nombre total d'acquisitions
 * @param[out] error_count Nombre d'erreurs
 * @param[out] last_update_ms Timestamp dernière mise à jour
 * @return ESP_OK si succès
 */
esp_err_t sensor_manager_get_stats(sensor_type_t type, 
                                    uint32_t *total_acquisitions,
                                    uint32_t *error_count,
                                    uint32_t *last_update_ms);

/**
 * @brief Formater les données d'un capteur en JSON
 * 
 * @param type Type de capteur
 * @param[out] json_buffer Buffer où écrire le JSON
 * @param buffer_size Taille du buffer
 * @return Nombre de caractères écrits, ou -1 si erreur
 */
int sensor_manager_format_json(sensor_type_t type, char *json_buffer, size_t buffer_size);

/**
 * @brief Activer/désactiver le logging automatique
 * 
 * @param enable true pour activer
 */
void sensor_manager_set_logging(bool enable);

/**
 * @brief Arrêter le gestionnaire de capteurs
 * 
 * Stoppe toutes les tâches d'acquisition et libère les ressources.
 */
void sensor_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_MANAGER_H
