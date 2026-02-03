/**
 * @file conversion_config.h
 * @brief Configuration des lois de conversion ADC → grandeurs physiques
 * 
 * Ce fichier définit les conversions pour transformer les valeurs brutes des ADC
 * en unités physiques (V, °C, mA, bar, etc.) sans recompilation du code.
 * Modifiez simplement les paramètres ci-dessous et recompilez.
 */

#ifndef CONVERSION_CONFIG_H
#define CONVERSION_CONFIG_H

#include <stdint.h>

/* ============================================================================
 * TYPES DE CONVERSION SUPPORTÉS
 * ============================================================================ */

typedef enum {
    CONV_NONE = 0,          // Aucune conversion (valeur brute)
    CONV_LINEAR,            // Linéaire : y = a*x + b
    CONV_POLYNOMIAL_2,      // Polynôme degré 2 : y = a*x² + b*x + c
    CONV_POLYNOMIAL_3,      // Polynôme degré 3 : y = a*x³ + b*x² + c*x + d
    CONV_LOOKUP_TABLE,      // Table de correspondance (interpolation linéaire)
} conversion_type_t;

/* ============================================================================
 * STRUCTURE DE CONFIGURATION D'UN CANAL
 * ============================================================================ */

typedef struct {
    const char *name;           // Nom du canal (ex: "Température moteur")
    const char *unit;           // Unité physique (ex: "°C", "mA", "bar")
    conversion_type_t type;     // Type de conversion
    
    // Paramètres conversion linéaire (CONV_LINEAR)
    struct {
        float a;                // Coefficient multiplicateur
        float b;                // Offset
    } linear;
    
    // Paramètres conversion polynomiale (CONV_POLYNOMIAL_2/3)
    struct {
        float a, b, c, d;       // Coefficients (d inutilisé pour degré 2)
    } poly;
    
    // Table de correspondance (CONV_LOOKUP_TABLE)
    struct {
        uint16_t *raw_values;   // Valeurs ADC
        float *phys_values;     // Valeurs physiques correspondantes
        uint8_t count;          // Nombre de points
    } lut;
    
} channel_conversion_t;

/* ============================================================================
 * CONFIGURATION ADS7128 (12-bit, 8 canaux)
 * Résolution : 4096 niveaux (0-4095)
 * ============================================================================ */

// Exemple : Capteur de température PT1000 (0-100°C)
static uint16_t temp_lut_raw[] = {0, 819, 1638, 2458, 3277, 4095};
static float temp_lut_phys[] = {0.0, 20.0, 40.0, 60.0, 80.0, 100.0};

// Exemple : Capteur de pression 4-20mA (0-10 bar)
// ADC mesure tension sur 250Ω : 1V = 4mA, 5V = 20mA
// Résolution : 4095/5V = 819 LSB/V
// 4mA (1V) = 819, 20mA (5V) = 4095

static const channel_conversion_t ads7128_config[8] = {
    // Canal 0 : Température moteur (PT1000 avec table)
    {
        .name = "Temp_Moteur",
        .unit = "°C",
        .type = CONV_LOOKUP_TABLE,
        .lut = {
            .raw_values = temp_lut_raw,
            .phys_values = temp_lut_phys,
            .count = 6
        }
    },
    
    // Canal 1 : Pression hydraulique (4-20mA → 0-10 bar)
    {
        .name = "Pression_Hydr",
        .unit = "bar",
        .type = CONV_LINEAR,
        .linear = {
            .a = 10.0 / (4095.0 - 819.0),   // (10 bar) / (20mA - 4mA en LSB)
            .b = -10.0 * 819.0 / (4095.0 - 819.0)  // Offset pour 4mA = 0 bar
        }
    },
    
    // Canal 2 : Tension batterie (diviseur 1:10, 0-60V)
    {
        .name = "Tension_Batt",
        .unit = "V",
        .type = CONV_LINEAR,
        .linear = {
            .a = 60.0 / 4095.0,  // 60V pleine échelle
            .b = 0.0
        }
    },
    
    // Canal 3 : Courant moteur (capteur Hall ACS712 30A)
    // Vout = 2.5V @ 0A, sensibilité 66mV/A
    // ADC : 2.5V = 2048 LSB, 66mV/A = 54 LSB/A
    {
        .name = "Courant_Mot",
        .unit = "A",
        .type = CONV_LINEAR,
        .linear = {
            .a = 1.0 / 54.0,     // 1A = 54 LSB
            .b = -2048.0 / 54.0  // 0A = 2048 LSB (2.5V)
        }
    },
    
    // Canaux 4-7 : Non utilisés (valeur brute)
    {.name = "ADC_CH4", .unit = "LSB", .type = CONV_NONE},
    {.name = "ADC_CH5", .unit = "LSB", .type = CONV_NONE},
    {.name = "ADC_CH6", .unit = "LSB", .type = CONV_NONE},
    {.name = "ADC_CH7", .unit = "LSB", .type = CONV_NONE},
};

/* ============================================================================
 * CONFIGURATION ADS1119 #1 (16-bit, 4 canaux, 0x40)
 * Résolution : 32768 niveaux (±2.048V en mode différentiel)
 * ============================================================================ */

static const channel_conversion_t ads1119_1_config[4] = {
    // Canal 0 : Thermocouple type K (-200 à +1200°C)
    // Linéarisation simplifiée : 41µV/°C
    // Résolution : 32768 / 2.048V = 16000 LSB/V = 0.656 LSB/°C
    {
        .name = "Thermocouple_K",
        .unit = "°C",
        .type = CONV_LINEAR,
        .linear = {
            .a = 1.0 / 0.656,    // °C par LSB
            .b = 0.0
        }
    },
    
    // Canal 1 : Cellule de charge (0-500 kg)
    // Pont de Wheatstone 2mV/V à 10V excitation → 10mV pleine échelle
    // 10mV = 164 LSB
    {
        .name = "Poids",
        .unit = "kg",
        .type = CONV_LINEAR,
        .linear = {
            .a = 500.0 / 164.0,  // 500 kg pour 10mV
            .b = 0.0
        }
    },
    
    // Canaux 2-3 : Non utilisés
    {.name = "ADS1_CH2", .unit = "LSB", .type = CONV_NONE},
    {.name = "ADS1_CH3", .unit = "LSB", .type = CONV_NONE},
};

/* ============================================================================
 * CONFIGURATION ADS1119 #2 (16-bit, 4 canaux, 0x41)
 * ============================================================================ */

static const channel_conversion_t ads1119_2_config[4] = {
    // Canal 0 : Niveau de liquide (capteur 0-5V → 0-2m)
    {
        .name = "Niveau_Eau",
        .unit = "m",
        .type = CONV_LINEAR,
        .linear = {
            .a = 2.0 / 32768.0,  // 2m pour 5V (pleine échelle)
            .b = 0.0
        }
    },
    
    // Canaux 1-3 : Réservés
    {.name = "ADS2_CH1", .unit = "LSB", .type = CONV_NONE},
    {.name = "ADS2_CH2", .unit = "LSB", .type = CONV_NONE},
    {.name = "ADS2_CH3", .unit = "LSB", .type = CONV_NONE},
};

/* ============================================================================
 * FONCTIONS D'ACCÈS (inline pour performance)
 * ============================================================================ */

/**
 * @brief Obtenir la configuration de conversion pour un canal ADS7128
 */
static inline const channel_conversion_t* get_ads7128_conversion(uint8_t channel) {
    return (channel < 8) ? &ads7128_config[channel] : NULL;
}

/**
 * @brief Obtenir la configuration de conversion pour un canal ADS1119 #1
 */
static inline const channel_conversion_t* get_ads1119_1_conversion(uint8_t channel) {
    return (channel < 4) ? &ads1119_1_config[channel] : NULL;
}

/**
 * @brief Obtenir la configuration de conversion pour un canal ADS1119 #2
 */
static inline const channel_conversion_t* get_ads1119_2_conversion(uint8_t channel) {
    return (channel < 4) ? &ads1119_2_config[channel] : NULL;
}

#endif // CONVERSION_CONFIG_H
