# Guide Développeur - APRU40

## Table des matières
1. [Vue d'ensemble de l'architecture](#vue-densemble-de-larchitecture)
2. [Structure du projet](#structure-du-projet)
3. [Composants principaux](#composants-principaux)
4. [Configuration du système](#configuration-du-système)
5. [Gestion des acquisitions ADC](#gestion-des-acquisitions-adc)
6. [Communication réseau](#communication-réseau)
7. [Guide de développement](#guide-de-développement)
8. [Mise à jour OTA](#mise-à-jour-ota)

---

## Vue d'ensemble de l'architecture

Le projet APRU40 est conçu avec une architecture modulaire permettant de gérer facilement un réseau de capteurs avec des nœuds (nodes) et des passerelles (gateways).

### Principes de conception

- **Configuration centralisée** : Un seul fichier `node_config.h` définit le mode et les paramètres
- **Modularité** : Chaque périphérique est géré par un composant dédié
- **Conversions configurables** : Les formules de conversion ADC sont définies sans recompilation
- **Scalabilité** : Architecture prête pour jusqu'à 30 nœuds
- **OTA Ready** : Structure préparée pour les mises à jour à distance via ESP-NOW

### Architecture en couches

**Nœuds (Nodes)** :
```
┌─────────────────────────────────────────────────────┐
│                   Application                        │
│                    (main.c)                          │
└─────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────┐
│              Gestionnaire de capteurs                │
│               (sensor_manager)                       │
│   ┌─────────────┬──────────────┬─────────────┐     │
│   │  ADS7128    │   ADS1119    │   ADS1119   │     │
│   │   Task      │    #1 Task   │   #2 Task   │     │
│   └─────────────┴──────────────┴─────────────┘     │
└─────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────┐
│              Drivers périphériques                   │
│   ┌──────────┬──────────┬──────────┬──────────┐    │
│   │ ADS7128  │ ADS1119  │ TCA9537  │  I2C Bus │    │
│   └──────────┴──────────┴──────────┴──────────┘    │
└─────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────┐
│    Communication (ESP-NOW + BT pour scanner)        │
│   ┌──────────────────┬──────────────────────┐       │
│   │  ESP-NOW Secure  │  Bluetooth SPP       │       │
│   │  (vers Gateway)  │  (Zebra DS2278)      │       │
│   └──────────────────┴──────────────────────┘       │
└─────────────────────────────────────────────────────┘
```

**Passerelles (Gateways)** :
```
┌─────────────────────────────────────────────────────┐
│              Application Gateway                     │
│                    (main.c)                          │
└─────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────┐
│           Gestionnaire de données nœuds              │
│         (buffer circulaire, agrégation)              │
└─────────────────────────────────────────────────────┘
                          │
┌─────────────────────────────────────────────────────┐
│                  Communication                       │
│   ┌──────────────────┬──────────────────────┐       │
│   │  ESP-NOW Secure  │   Ethernet + MQTT    │       │
│   │  (depuis Nodes)  │   (Mosquitto/TLS1.3) │       │
│   └──────────────────┴──────────────────────┘       │
└─────────────────────────────────────────────────────┘
```

---

## Structure du projet

```
APRU40/
├── include/                      # Headers de configuration
│   ├── node_config.h            # Configuration nœud/passerelle
│   └── conversion_config.h      # Formules de conversion ADC
│
├── main/                         # Application principale
│   ├── main.c                   # Point d'entrée
│   └── CMakeLists.txt
│
├── components/                   # Composants modulaires
│   ├── sensor_manager/          # Gestionnaire d'acquisitions
│   │   ├── sensor_manager.c
│   │   ├── sensor_manager.h
│   │   └── CMakeLists.txt
│   │
│   ├── ads7128/                 # Driver ADC 12-bit 8 canaux
│   │   ├── ads7128.c
│   │   ├── include/ads7128.h
│   │   └── CMakeLists.txt
│   │
│   ├── ads1119/                 # Driver ADC 16-bit 4 canaux
│   │   ├── ads1119.c
│   │   ├── include/ads1119.h
│   │   └── CMakeLists.txt
│   │
│   ├── tca9537/                 # Driver GPIO expander
│   │   ├── tca9537.c
│   │   ├── include/tca9537.h
│   │   └── CMakeLists.txt
│   │
│   ├── i2c_bus/                 # Gestion bus I2C
│   │   ├── i2c_bus.c
│   │   ├── include/i2c_bus.h
│   │   └── CMakeLists.txt
│   │
│   ├── esp_now_secure/          # Communication ESP-NOW
│   │   ├── esp_now_secure.c
│   │   ├── include/esp_now_secure.h
│   │   └── CMakeLists.txt
│   │
│   └── bluetooth_spp/           # Communication Bluetooth
│       ├── bluetooth_spp.c
│       ├── include/bluetooth_spp.h
│       └── CMakeLists.txt
│
├── platformio.ini               # Configuration PlatformIO
├── CMakeLists.txt               # Configuration CMake racine
├── partitions.csv               # Table de partitions personnalisée
│
└── Documentation
    ├── ARCHITECTURE.md          # Architecture détaillée
    ├── CONFIG_OTA_GUIDE.md      # Guide OTA et config dynamique
    └── ReadMeDevelopper.md      # Ce fichier
```

---

## Composants principaux

### 1. Configuration centrale (`include/node_config.h`)

Ce fichier est le **point de configuration unique** pour tout le système.

```c
// Définir le mode du dispositif
#define NODE_MODE MODE_NODE      // ou MODE_GATEWAY

// Activer/désactiver les périphériques
#define ENABLE_ADS7128     1     // ADC 12-bit 8 canaux
#define ENABLE_ADS1119_1   1     // ADC 16-bit 4 canaux #1
#define ENABLE_ADS1119_2   0     // ADC 16-bit 4 canaux #2
#define ENABLE_TCA9537     1     // GPIO expander
#define ENABLE_BLUETOOTH   1     // Bluetooth SPP
#define ENABLE_ESP_NOW     1     // ESP-NOW sécurisé

// Périodes d'acquisition (en ms)
#define ADS7128_ACQUISITION_PERIOD_MS   1000
#define ADS1119_ACQUISITION_PERIOD_MS   2000

// Clés de sécurité
#define AES_KEY_PRIMARY    "MySecretAESKey16"
#define HMAC_KEY_PRIMARY   "MySecretHMACKey32bytes123456"
```

**Changement de configuration** :
- Modifier `NODE_MODE` pour changer entre nœud et passerelle
- Activer/désactiver les périphériques avec `ENABLE_*`
- Ajuster les périodes d'acquisition selon vos besoins
- **Compiler et flasher** pour appliquer les changements

### 2. Conversions ADC (`include/conversion_config.h`)

Définit les formules de conversion des valeurs brutes ADC vers des unités physiques.

#### Types de conversion supportés

**a) Linéaire** (type `CONV_LINEAR`)
```c
// Formule : physical = a * raw + b
DEFINE_LINEAR_CONVERSION(conv_4_20mA, 0.00488, 4.0);
// Exemple : 0-4095 → 4-20 mA
```

**b) Polynomiale degré 2** (type `CONV_POLYNOMIAL_2`)
```c
// Formule : physical = a*raw² + b*raw + c
DEFINE_POLY2_CONVERSION(conv_thermocouple, 2.5e-6, 0.041, -50.0);
// Exemple : Thermocouple K
```

**c) Polynomiale degré 3** (type `CONV_POLYNOMIAL_3`)
```c
// Formule : physical = a*raw³ + b*raw² + c*raw + d
DEFINE_POLY3_CONVERSION(conv_complex, 1e-9, 5e-6, 0.05, 10.0);
```

**d) Table de correspondance (LUT)** (type `CONV_LOOKUP_TABLE`)
```c
static const conversion_lut_t pt1000_lut = {
    .num_points = 5,
    .raw_values = {100, 500, 1000, 2000, 4000},
    .physical_values = {-50.0, 0.0, 50.0, 100.0, 200.0}
};
```

#### Assignation aux canaux

```c
// Configuration ADS7128 (canaux 0-7)
static const adc_channel_conversion_t g_ads7128_conversions[8] = {
    {0, &conv_4_20mA},           // Canal 0 : Capteur 4-20mA
    {1, &conv_thermocouple},     // Canal 1 : Thermocouple
    {2, NULL},                   // Canal 2 : Valeur brute
    // ...
};

// Configuration ADS1119 #1 (canaux 0-3)
static const adc_channel_conversion_t g_ads1119_1_conversions[4] = {
    {0, &conv_pt1000_lut},       // Canal 0 : PT1000 avec LUT
    {1, &conv_4_20mA},           // Canal 1 : Capteur 4-20mA
    // ...
};
```

### 3. Gestionnaire de capteurs (`sensor_manager`)

Le `sensor_manager` orchestre automatiquement toutes les acquisitions ADC.

#### Fonctionnalités

- **Tâches FreeRTOS dédiées** : Une tâche par ADC pour acquisition périodique
- **Conversion automatique** : Applique les formules définies dans `conversion_config.h`
- **Thread-safe** : Mutex pour protéger les ressources partagées
- **Logs structurés** : Affichage clair des valeurs brutes et converties

#### API publique

```c
// Initialisation (appelée depuis main.c)
esp_err_t sensor_manager_init(void);

// Démarrage des acquisitions
esp_err_t sensor_manager_start_all(void);

// Arrêt des acquisitions
void sensor_manager_stop_all(void);

// Lecture des dernières valeurs converties
esp_err_t sensor_manager_get_ads7128_data(float *data, size_t *count);
esp_err_t sensor_manager_get_ads1119_1_data(float *data, size_t *count);
esp_err_t sensor_manager_get_ads1119_2_data(float *data, size_t *count);
```

#### Exemple de sortie

```
[ADS7128] Canal 0: raw=2048 → 12.00 mA
[ADS7128] Canal 1: raw=1500 → 45.3 °C
[ADS1119_1] Canal 0: raw=15000 → 95.2 °C (PT1000)
```

---

## Configuration du système

### Mode Nœud (NODE)

Un **nœud** acquiert des données de capteurs et les transmet à une passerelle.

**Configuration dans `node_config.h`** :
```c
#define NODE_MODE MODE_NODE

#define ENABLE_ADS7128     1     // Acquisition locale
#define ENABLE_ADS1119_1   1
#define ENABLE_BLUETOOTH   1     // Pour scanner Zebra DS2278 (si présent)
#define ENABLE_ESP_NOW     1     // Communication avec gateway
```

**Comportement** :
- Les tâches d'acquisition démarrent automatiquement
- Les données sont envoyées via ESP-NOW à la passerelle
- Cryptage AES + authentification HMAC
- **Bluetooth SPP** : Uniquement pour nœuds avec scanner QR code Zebra DS2278
  - Réception des codes-barres scannés
  - Transmission des codes avec les données de capteurs
- Mode basse consommation possible (deep sleep entre acquisitions)

### Mode Passerelle (GATEWAY)

Une **passerelle** collecte les données de tous les nœuds et les publie vers le cloud via MQTT.

**Configuration dans `node_config.h`** :
```c
#define NODE_MODE MODE_GATEWAY

#define ENABLE_ADS7128     0     // Pas d'acquisition locale
#define ENABLE_ADS1119_1   0
#define ENABLE_BLUETOOTH   0     // Pas de BT sur gateway
#define ENABLE_ESP_NOW     1     // Réception depuis nœuds
#define ENABLE_ETHERNET    1     // Communication MQTT
```

**Comportement** :
- Écoute les trames ESP-NOW des nœuds
- Stocke les données en mémoire (buffer circulaire)
- **Ethernet** : Connexion réseau filaire (ESP32-POE-ISO)
- **MQTT** : Publication vers broker Mosquitto avec TLS 1.3
  - Topic données : `apru40/gateway/{gateway_id}/data`
  - Topic commandes : `apru40/gateway/{gateway_id}/cmd`
  - Topic configuration : `apru40/gateway/{gateway_id}/config`
- Peut logger sur carte SD (à implémenter)

---

## Gestion des acquisitions ADC

### Cycle de vie d'une acquisition

```
┌──────────────┐
│ Initialiser  │  sensor_manager_init()
│  les ADCs    │  → ads7128_init(), ads1119_init()
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  Démarrer    │  sensor_manager_start_all()
│  les tâches  │  → xTaskCreate() pour chaque ADC
└──────┬───────┘
       │
       ▼
┌──────────────────────────────┐
│   Boucle d'acquisition       │
│   (périodique dans tâche)    │
│                              │
│  1. Attendre période         │ ← vTaskDelay(period)
│  2. Pour chaque canal :      │
│     - Set channel            │ ← ads7128_set_adc_channel()
│     - Read raw value         │ ← ads7128_read_adc_raw12()
│     - Apply conversion       │ ← apply_conversion()
│     - Store result           │
│  3. Log values               │
│  4. Envoyer via ESP-NOW      │ (si MODE_NODE)
└──────────────────────────────┘
```

### Ajouter un nouveau type d'ADC

1. **Créer le driver** dans `components/mon_adc/`
```c
// include/mon_adc.h
typedef struct {
    i2c_port_t port;
    uint8_t addr;
} mon_adc_t;

esp_err_t mon_adc_init(mon_adc_t *dev, i2c_port_t port, uint8_t addr);
esp_err_t mon_adc_set_channel(mon_adc_t *dev, uint8_t channel);
esp_err_t mon_adc_read_raw(mon_adc_t *dev, uint16_t *raw);
```

2. **Ajouter dans `node_config.h`**
```c
#define ENABLE_MON_ADC          1
#define MON_ADC_I2C_ADDR        0x48
#define MON_ADC_ACQUISITION_PERIOD_MS  1000
```

3. **Ajouter dans `conversion_config.h`**
```c
static const adc_channel_conversion_t g_mon_adc_conversions[4] = {
    {0, &conv_ma_conversion},
    // ...
};
```

4. **Ajouter dans `sensor_manager.c`**
```c
#if ENABLE_MON_ADC
static mon_adc_t g_mon_adc;

static void task_mon_adc(void *pvParameters) {
    while (1) {
        for (uint8_t ch = 0; ch < 4; ch++) {
            uint16_t raw;
            mon_adc_set_channel(&g_mon_adc, ch);
            mon_adc_read_raw(&g_mon_adc, &raw);
            // Conversion et stockage
        }
        vTaskDelay(pdMS_TO_TICKS(MON_ADC_ACQUISITION_PERIOD_MS));
    }
}
#endif
```

---

## Communication réseau

### ESP-NOW Sécurisé (Nœuds → Passerelle)

**Architecture** :
- **Nœuds → Passerelle** : Envoi périodique des données acquises
- **Cryptage AES-128** : Confidentialité des données
- **Authentification HMAC-SHA256** : Intégrité et authentification

**Format de trame** :
```c
typedef struct {
    uint8_t node_id;              // ID du nœud (1-30)
    uint32_t timestamp;           // Timestamp acquisition
    uint8_t adc_type;             // ADS7128=1, ADS1119_1=2, ...
    uint8_t num_channels;         // Nombre de canaux
    float values[16];             // Valeurs converties
    char qr_code[64];             // Code QR scanné (si présent)
    uint8_t hmac[32];             // Signature HMAC
} __attribute__((packed)) espnow_data_packet_t;
```

**Configuration** :
```c
// node_config.h
#define GATEWAY_MAC_ADDR {0x24, 0x0A, 0xC4, 0xXX, 0xXX, 0xXX}
#define NODE_ID 1  // Unique pour chaque nœud

// Clés partagées (identiques sur nœuds et gateway)
#define AES_KEY_PRIMARY    "MySecretAESKey16"
#define HMAC_KEY_PRIMARY   "MySecretHMACKey32bytes123456"
```

### Bluetooth SPP (Nœuds avec scanner uniquement)

**Utilisation** :
- Connexion au scanner de code-barres Zebra DS2278
- Réception des codes QR/codes-barres scannés
- Mode SPP (Serial Port Profile) Bluetooth Classic

**Configuration du scanner Zebra** :
```
- Mode: SPP (Serial Port Profile)
- Baudrate: 9600
- Suffixe: CR+LF (\r\n)
- Appairage: Code PIN ou auto
```

**Exemple de traitement** :
```c
// Callback réception données Bluetooth
void scanner_data_callback(const char *data, size_t len) {
    // data contient le code QR scanné
    ESP_LOGI("SCANNER", "QR Code: %s", data);
    
    // Associer code QR aux prochaines données de capteurs
    sensor_manager_attach_qr_code(data);
}
```

### MQTT/TLS 1.3 (Passerelle → Cloud)

**Architecture** :
- **Protocole** : MQTT v3.1.1 ou v5.0
- **Transport** : Ethernet filaire (ESP32-POE-ISO)
- **Sécurité** : TLS 1.3 avec certificats X.509
- **Broker** : Mosquitto

**Topics MQTT** :
```
apru40/gateway/{gateway_id}/data          → Publication données agrégées
apru40/gateway/{gateway_id}/status        → Heartbeat gateway
apru40/gateway/{gateway_id}/cmd           → Réception commandes
apru40/gateway/{gateway_id}/config        → Réception configurations
apru40/node/{node_id}/config              → Config spécifique nœud
apru40/node/{node_id}/ota                 → Firmware OTA pour nœud
```

**Payload données** (JSON) :
```json
{
  "gateway_id": "GW001",
  "timestamp": 1738454400,
  "nodes": [
    {
      "node_id": 1,
      "qr_code": "PROD12345",
      "sensors": {
        "ads7128": {
          "ch0": {"raw": 2048, "value": 12.5, "unit": "mA"},
          "ch1": {"raw": 1500, "value": 45.3, "unit": "°C"}
        },
        "ads1119_1": {
          "ch0": {"raw": 15000, "value": 95.2, "unit": "°C"}
        }
      },
      "rssi": -45,
      "battery": 3.7
    }
  ]
}
```

**Payload configuration** (JSON) :
```json
{
  "cmd": "update_config",
  "target": "node",
  "node_id": 1,
  "config": {
    "ads7128_period_ms": 2000,
    "ads1119_period_ms": 5000,
    "conversion_ch0": {
      "type": "linear",
      "a": 0.00488,
      "b": 4.0
    }
  }
}
```

**Configuration TLS** :
```c
// node_config.h (Gateway)
#define MQTT_BROKER_URI     "mqtts://broker.example.com:8883"
#define MQTT_CLIENT_ID      "APRU40_GW001"
#define MQTT_USERNAME       "apru40_gateway"
#define MQTT_PASSWORD       "***"

// Certificats (à stocker dans SPIFFS ou intégrés)
#define MQTT_CA_CERT_PATH   "/spiffs/ca.crt"
#define MQTT_CLIENT_CERT    "/spiffs/client.crt"
#define MQTT_CLIENT_KEY     "/spiffs/client.key"
```

**Exemple de connexion** :
```c
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = MQTT_BROKER_URI,
    .broker.verification.certificate = ca_cert_pem,
    .credentials = {
        .authentication = {
            .certificate = client_cert_pem,
            .key = client_key_pem,
        },
        .username = MQTT_USERNAME,
        .client_id = MQTT_CLIENT_ID,
    },
    .session.protocol_ver = MQTT_PROTOCOL_V_5,  // MQTT v5
};

esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
esp_mqtt_client_start(client);
```

---

## Guide de développement

### Prérequis

- **PlatformIO** : Installé dans VS Code
- **ESP-IDF 5.5.0** : Via PlatformIO
- **Carte** : ESP32-POE-ISO (Olimex)
- **Python 3.x** : Pour les scripts de build

### Compiler le projet

1. **Ouvrir le projet** dans VS Code
2. **Sélectionner l'environnement** : `esp32-poe-iso` (dans la barre PlatformIO)
3. **Build** : Ctrl+Shift+B ou menu PlatformIO → Build

Ou en ligne de commande :
```bash
cd APRU40
platformio run --environment esp32-poe-iso
```

### Flasher le firmware

**Via USB** :
```bash
platformio run --target upload --environment esp32-poe-iso
```

**Via JTAG** (OpenOCD) :
```bash
platformio run --target upload --upload-port /dev/ttyUSB0
```

### Moniteur série

```bash
platformio device monitor --baud 115200
```

Ou dans VS Code : PlatformIO → Monitor

### Workflow de développement typique

1. **Modifier la configuration** : Éditer `node_config.h` ou `conversion_config.h`
2. **Compiler** : Vérifier les erreurs
3. **Flasher** : Sur le dispositif cible
4. **Tester** : Observer les logs via moniteur série
5. **Valider** : Vérifier les valeurs converties et la communication

### Debugging

**Logs ESP-IDF** :
```c
ESP_LOGI("MON_TAG", "Message d'information");
ESP_LOGW("MON_TAG", "Avertissement");
ESP_LOGE("MON_TAG", "Erreur : %s", esp_err_to_name(err));
```

**Niveaux de log** (dans `platformio.ini`) :
```ini
build_flags = 
    -D LOG_LOCAL_LEVEL=ESP_LOG_VERBOSE  ; Tous les logs
```

**JTAG Debugging** :
```ini
debug_tool = esp-prog
debug_init_break = tbreak app_main
```

### Tests unitaires

Structure recommandée :
```
test/
├── test_ads7128/
│   └── test_ads7128.c
├── test_conversion/
│   └── test_conversion.c
└── test_espnow/
    └── test_espnow.c
```

Exécution :
```bash
platformio test --environment esp32-poe-iso
```

---

## Mise à jour OTA

### Architecture OTA via MQTT

Le système utilise MQTT comme canal principal pour les mises à jour OTA.

#### 1. OTA Firmware (via MQTT → ESP-NOW)

**Principe** :
- Serveur cloud publie firmware sur topic MQTT
- Gateway télécharge firmware via MQTT/TLS
- Gateway diffuse firmware par blocs ESP-NOW vers nœuds
- Nœuds écrivent dans partition OTA et redémarrent

**Partitions** (voir `partitions.csv`) :
```
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x300000,  # 3MB app principale
app1,     app,  ota_1,   0x310000,0x300000,  # 3MB app backup OTA
spiffs,   data, spiffs,  0x610000,0x1F0000,  # 1.9MB stockage
```

**Topics MQTT OTA** :
```
apru40/ota/firmware/{version}/info        → Métadonnées firmware
apru40/ota/firmware/{version}/url         → URL téléchargement
apru40/gateway/{gateway_id}/ota/status    → Statut téléchargement
apru40/node/{node_id}/ota/progress        → Progression flashage nœud
```

**Payload info firmware** (JSON) :
```json
{
  "version": "1.2.3",
  "target": "node",
  "size": 2097152,
  "sha256": "abc123...",
  "signature": "RSA_signature_base64",
  "url": "https://firmware.example.com/apru40_node_v1.2.3.bin",
  "mandatory": false,
  "rollout": {
    "strategy": "gradual",
    "percentage": 10
  }
}
```

**À implémenter** :
```c
// components/mqtt_manager/mqtt_ota.c
esp_err_t mqtt_ota_check_update(void);
esp_err_t mqtt_ota_download_firmware(const char *url);
esp_err_t mqtt_ota_flash_nodes(const uint8_t *node_ids, size_t count);

// components/config_manager/config_manager.c
esp_err_t ota_begin_update(size_t image_size);
esp_err_t ota_write_block(const uint8_t *data, size_t len);
esp_err_t ota_finish_update(void);
```

#### 2. OTA Configuration (via MQTT → ESP-NOW)

**Principe** :
- Serveur cloud publie nouvelle config sur topic MQTT
- Gateway reçoit config et valide
- Gateway envoie config via ESP-NOW au nœud ciblé
- Nœud écrit dans NVS et applique sans redémarrage

**Topics MQTT configuration** :
```
apru40/gateway/{gateway_id}/config        → Config gateway
apru40/node/{node_id}/config              → Config nœud spécifique
apru40/broadcast/config                   → Config broadcast (tous nœuds)
```

**Payload configuration** (JSON) :
```json
{
  "node_id": 1,
  "timestamp": 1738454400,
  "config": {
    "acquisition": {
      "ads7128_period_ms": 2000,
      "ads1119_period_ms": 5000
    },
    "conversions": {
      "ads7128_ch0": {
        "type": "linear",
        "a": 0.00488,
        "b": 4.0,
        "unit": "mA"
      },
      "ads7128_ch1": {
        "type": "polynomial_2",
        "a": 2.5e-6,
        "b": 0.041,
        "c": -50.0,
        "unit": "°C"
      }
    },
    "thresholds": {
      "ads7128_ch0_min": 4.0,
      "ads7128_ch0_max": 20.0
    }
  },
  "signature": "HMAC_signature"
}
```

**Exemple de config dynamique** :
```c
// Callback réception config MQTT
void mqtt_config_handler(const char *topic, const char *payload) {
    cJSON *json = cJSON_Parse(payload);
    
    // Extraire node_id
    int node_id = cJSON_GetObjectItem(json, "node_id")->valueint;
    
    if (node_id == 0) {  // Config pour gateway
        apply_gateway_config(json);
    } else {  // Config pour nœud spécifique
        // Envoyer via ESP-NOW
        espnow_send_config(node_id, payload);
    }
}

// Sur le nœud : réception config
void espnow_config_handler(const uint8_t *data, size_t len) {
    // Écrire dans NVS
    nvs_handle_t handle;
    nvs_open("config", NVS_READWRITE, &handle);
    nvs_set_blob(handle, "sensor_config", data, len);
    nvs_commit(handle);
    nvs_close(handle);
    
    // Appliquer sans redémarrage
    sensor_manager_reload_config();
}
```

**Paramètres configurables dynamiquement** :
- Périodes d'acquisition
- Coefficients de conversion (a, b, c pour formules)
- Seuils d'alarme (min/max)
- Identifiant de nœud
- Adresse MAC gateway (changement de passerelle)
- Clés de cryptage (rotation périodique)

### Procédure OTA firmware complète

```
┌────────────┐         ┌─────────────┐         ┌─────────────┐         ┌──────────┐
│  Cloud     │         │   Broker    │         │   Gateway   │         │  Nœud 1  │
│  Server    │         │  Mosquitto  │         │  (ESP32)    │         │ (ESP32)  │
└─────┬──────┘         └──────┬──────┘         └──────┬──────┘         └────┬─────┘
      │                       │                       │                      │
      │ 1. Publish firmware   │                       │                      │
      │    metadata (JSON)    │                       │                      │
      ├──────────────────────>│                       │                      │
      │   Topic: apru40/ota/  │                       │                      │
      │   firmware/1.2.3/info │                       │                      │
      │                       │                       │                      │
      │                       │ 2. Forward to gateway │                      │
      │                       ├──────────────────────>│                      │
      │                       │   (MQTT/TLS 1.3)      │                      │
      │                       │                       │                      │
      │                       │                       │ 3. Download firmware │
      │                       │                       │    from URL (HTTPS)  │
      │<──────────────────────┼───────────────────────┤                      │
      │                       │                       │                      │
      │                       │                       │ 4. Validate SHA256   │
      │                       │                       │    & signature       │
      │                       │                       │                      │
      │                       │ 5. Publish status OK  │                      │
      │                       │<──────────────────────┤                      │
      │                       │                       │                      │
      │                       │                       │ 6. Announce OTA      │
      │                       │                       │    (ESP-NOW)         │
      │                       │                       ├─────────────────────>│
      │                       │                       │                      │
      │                       │                       │ 7. Request block 0   │
      │                       │                       │<─────────────────────┤
      │                       │                       │                      │
      │                       │                       │ 8-N. Send blocks     │
      │                       │                       ├─────────────────────>│
      │                       │                       │    (ESP-NOW 250B)    │
      │                       │                       │                      │
      │                       │                       │ N+1. Validate & boot │
      │                       │                       │<─────────────────────┤
      │                       │                       │                      │
      │                       │ N+2. Publish success  │                      │
      │                       │<──────────────────────┤                      │
      │                       │                       │                      │
      │<──────────────────────┤                       │                      │
```

**Sécurité OTA** :
- **Transport** : TLS 1.3 pour MQTT (gateway ↔ broker)
- **Signature firmware** : RSA-2048 ou ECDSA P-256
- **Checksum** : SHA-256 vérification intégrité
- **Authentification** : HMAC pour paquets ESP-NOW
- **Rollback** : Partition OTA backup + watchdog
- **Certificats** : Rotation périodique des certificats TLS

---

## Bonnes pratiques

### Configuration

✅ **À FAIRE** :
- Toujours définir `NODE_MODE` avant compilation
- Désactiver périphériques non utilisés (économie mémoire/énergie)
- Ajuster périodes d'acquisition selon besoins applicatifs
- Documenter les formules de conversion dans le code
- Stocker certificats TLS dans SPIFFS avec permissions restreintes
- Activer ENABLE_BLUETOOTH uniquement sur nœuds avec scanner Zebra

❌ **À ÉVITER** :
- Mélanger configuration nœud/gateway sur même firmware
- Périodes < 100ms (surcharge CPU)
- Clés de sécurité en clair dans le code (utiliser NVS pour prod)
- Certificats TLS hardcodés (utiliser SPIFFS + chiffrement)
- Activer Bluetooth sur gateway (inutile et consomme ressources)

### Développement

✅ **À FAIRE** :
- Vérifier retours d'erreur (`esp_err_t`)
- Logger les événements importants
- Utiliser mutexes pour ressources partagées (I2C, buffers)
- Tester conversions avec valeurs connues

❌ **À ÉVITER** :
- Appels bloquants dans ISR (interruptions)
- Allocations dynamiques dans tâches temps-réel
- Logs verbeux en production (baisse perf)

### Performance

**Optimisations I2C** :
```c
// Configuration I2C rapide
i2c_config.master.clk_speed = 400000;  // 400 kHz (Fast Mode)
```

**Optimisations tâches** :
```c
// Priorités FreeRTOS
#define TASK_PRIORITY_ADC      5  // Acquisition haute priorité
#define TASK_PRIORITY_COMM     4  // Communication moyenne priorité
#define TASK_PRIORITY_LOG      1  // Logs basse priorité
```

**Économie d'énergie (nœuds)** :
```c
// Deep sleep entre acquisitions (à implémenter)
esp_sleep_enable_timer_wakeup(ACQUISITION_PERIOD_MS * 1000);
esp_deep_sleep_start();
```

---

## Ressources supplémentaires

### Documentation

- **ESP-IDF** : https://docs.espressif.com/projects/esp-idf/en/v5.5/
- **PlatformIO** : https://docs.platformio.org/
- **FreeRTOS** : https://www.freertos.org/Documentation/

### Fichiers de référence

- [`ARCHITECTURE.md`](ARCHITECTURE.md) : Architecture détaillée avec exemples de code
- [`CONFIG_OTA_GUIDE.md`](CONFIG_OTA_GUIDE.md) : Guide OTA et config dynamique via NVS

### Datasheets

- **ADS7128** : ADC 12-bit 8 canaux I2C (Texas Instruments)
- **ADS1119** : ADC 16-bit 4 canaux I2C (Texas Instruments)
- **TCA9537** : GPIO Expander 4-bit I2C (Texas Instruments)
- **ESP32** : https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf

---

## Support et contribution

### Signaler un bug

1. Vérifier que c'est reproductible
2. Collecter logs complets (moniteur série)
3. Noter configuration utilisée (`node_config.h`)
4. Créer issue avec template :
```
**Description** : [Description claire]
**Configuration** : MODE_NODE, ADS7128 activé, période 1000ms
**Logs** : [Copier logs]
**Comportement attendu** : [Description]
**Comportement observé** : [Description]
```

### Contribuer

1. Fork le projet
2. Créer branche feature : `git checkout -b feature/ma-fonctionnalite`
3. Respecter style de code (voir `.clang-format`)
4. Tester sur hardware réel
5. Documenter dans code et README si nécessaire
6. Commit : `git commit -m "feat: ajout support ADC XYZ"`
7. Push et créer Pull Request

---

## FAQ

**Q : Comment changer un nœud en passerelle ?**  
R : Modifier `NODE_MODE` dans `node_config.h`, activer `ENABLE_ETHERNET`, désactiver `ENABLE_BLUETOOTH`, recompiler et flasher.

**Q : Puis-je avoir plusieurs passerelles ?**  
R : Oui, mais elles doivent avoir différents `gateway_id` MQTT et peuvent opérer en mode actif/passif ou répartition de charge.

**Q : Comment ajouter un 31ème nœud ?**  
R : Augmenter `MAX_NODES` dans `node_config.h` et recompiler la gateway. Vérifier aussi les limites du broker MQTT.

**Q : Les conversions ADC nécessitent-elles une recompilation ?**  
R : Non si vous utilisez la configuration via MQTT. Les coefficients peuvent être envoyés dynamiquement et stockés en NVS.

**Q : Quelle est la portée ESP-NOW ?**  
R : ~200m en ligne de vue. Utiliser nœuds intermédiaires comme répéteurs si besoin.

**Q : Comment configurer le scanner Zebra DS2278 ?**  
R : 
1. Appairage Bluetooth avec ESP32
2. Configurer mode SPP dans le scanner (scan barcode de config)
3. Suffixe CR+LF activé
4. Baudrate 9600
5. Le code scanné sera automatiquement associé aux données de capteurs

**Q : Quel broker MQTT utiliser ?**  
R : Mosquitto recommandé. Configuration TLS 1.3 obligatoire pour production. Exemple :
```bash
# mosquitto.conf
listener 8883
cafile /etc/mosquitto/ca.crt
certfile /etc/mosquitto/server.crt
keyfile /etc/mosquitto/server.key
tls_version tlsv1.3
require_certificate true
```

**Q : Comment sécuriser pour la production ?**  
R : 
- Stocker clés AES/HMAC dans NVS chiffré
- Activer Secure Boot ESP32
- Certificats TLS avec rotation périodique
- Authentification mutuelle MQTT (mTLS)
- Implémenter signature firmware OTA (RSA-2048)
- Désactiver logs verbeux
- Firewall sur broker Mosquitto (whitelist IPs gateways)

---

**Version** : 1.0  
**Date** : Février 2026  
**Auteur** : Projet APRU40  
**Licence** : [À définir]
