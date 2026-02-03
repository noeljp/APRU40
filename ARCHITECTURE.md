# Architecture modulaire APRU40

## 📐 Vue d'ensemble

Cette architecture permet de configurer chaque module ESP32 en **nœud capteur** ou **gateway** sans modifier le code principal, simplement en changeant des `#define` dans les fichiers de configuration.

```
┌─────────────────────────────────────────────────────────────┐
│                    APPLICATION (main.c)                      │
│  ─ Initialisation                                           │
│  ─ Tâches FreeRTOS (heartbeat, transmission)                │
│  ─ Callbacks (ESP-NOW, Bluetooth, capteurs)                 │
└────────┬────────────────────────────────────────────────────┘
         │
         ├──► node_config.h ──────────► Configuration centralisée
         │                              (mode, ID, périphériques)
         │
         ├──► sensor_manager ─────────► Acquisition automatique
         │    (tâches d'arrière-plan)   (conversions physiques)
         │                              
         ├──► conversion_config.h ────► Lois de conversion
         │                              (linéaire, polynomiale, LUT)
         │
         ├──► esp_now_secure ─────────► Communication sans fil
         │                              (AES-256 + HMAC)
         │
         ├──► bluetooth_spp ──────────► Scanner codes-barres
         │                              (Zebra DS2278)
         │
         └──► Drivers ADC/GPIO ───────► ads7128, ads1119, tca9537
                                        (via I2C bus)
```

## 🔧 Fichiers de configuration

### 1. `include/node_config.h` - Configuration du module

**Objectif** : Un seul fichier à modifier pour adapter chaque module

```c
// Définir le mode du module
#define NODE_MODE       MODE_NODE    // ou MODE_GATEWAY

// Identité
#define NODE_ID         1            // 1-254 (unique par nœud)
#define NODE_NAME       "APRU40-Node-01"

// Activer/désactiver périphériques
#define ENABLE_ADS7128      1
#define ENABLE_ADS1119_1    1
#define ENABLE_ADS1119_2    1
#define ENABLE_TCA9537      1
#define ENABLE_BLUETOOTH_SPP 1

// Périodes d'acquisition (ms)
#define ADC_ACQUISITION_PERIOD_MS   5000
#define ESP_NOW_TX_PERIOD_MS        10000

// Clés de sécurité ESP-NOW
#define AES_KEY { 0x2b, 0x7e, ... }
#define HMAC_KEY { 0xc0, 0x9f, ... }
```

**Utilisation** :
- **Nœud 1** : `NODE_MODE=MODE_NODE`, `NODE_ID=1`, tous périphériques actifs
- **Nœud 2** : `NODE_MODE=MODE_NODE`, `NODE_ID=2`, seulement ADS7128
- **Gateway** : `NODE_MODE=MODE_GATEWAY`, `NODE_ID=255`, pas d'ADC

### 2. `include/conversion_config.h` - Conversions physiques

**Objectif** : Définir comment convertir les valeurs ADC brutes en unités physiques (°C, bar, mA...) **sans recompiler le code métier**

**Types de conversion supportés** :
- `CONV_NONE` : Pas de conversion (valeur brute LSB)
- `CONV_LINEAR` : Linéaire `y = a*x + b`
- `CONV_POLYNOMIAL_2` : Polynôme degré 2 `y = a*x² + b*x + c`
- `CONV_POLYNOMIAL_3` : Polynôme degré 3
- `CONV_LOOKUP_TABLE` : Table de correspondance avec interpolation linéaire

**Exemple - Capteur de pression 4-20mA → 0-10 bar** :

```c
static const channel_conversion_t ads7128_config[8] = {
    // Canal 1 : Pression hydraulique
    {
        .name = "Pression_Hydr",
        .unit = "bar",
        .type = CONV_LINEAR,
        .linear = {
            .a = 10.0 / (4095.0 - 819.0),   // Pente
            .b = -10.0 * 819.0 / (4095.0 - 819.0)  // Offset
        }
    },
    // ...
};
```

**Exemple - Thermocouple avec table** :

```c
static uint16_t temp_lut_raw[] = {0, 819, 1638, 2458, 3277, 4095};
static float temp_lut_phys[] = {0.0, 20.0, 40.0, 60.0, 80.0, 100.0};

static const channel_conversion_t ads7128_config[8] = {
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
    // ...
};
```

**Ajout d'un nouveau capteur** :
1. Calculer les coefficients de conversion (datasheet capteur)
2. Modifier `conversion_config.h`
3. Recompiler → **Aucune modification du code métier**

## 🎯 Composant `sensor_manager`

### Rôle

Gestionnaire unifié de tous les capteurs ADC qui :
1. **Initialise automatiquement** tous les ADC activés dans `node_config.h`
2. **Crée des tâches FreeRTOS** pour acquisition périodique en arrière-plan
3. **Applique les conversions** physiques automatiquement
4. **Stocke les dernières valeurs** (thread-safe avec mutex)
5. **Génère du JSON** formaté pour ESP-NOW

### API publique

```c
// Initialisation (une seule fois au boot)
sensor_manager_config_t config = {
    .acquisition_period_ms = 5000,
    .callback = my_sensor_callback,  // Optionnel
    .enable_logging = true            // Logs auto dans console
};
sensor_manager_init(&config);

// Lecture des données (depuis n'importe quelle tâche)
sensor_readings_t readings;
sensor_manager_get_readings(SENSOR_TYPE_ADS7128, &readings);

// Valeur d'un canal spécifique
float temp;
sensor_manager_get_channel_value(SENSOR_TYPE_ADS7128, 0, &temp);
ESP_LOGI(TAG, "Température moteur: %.2f °C", temp);

// Génération JSON pour ESP-NOW
char json[512];
int len = sensor_manager_format_json(SENSOR_TYPE_ADS7128, json, sizeof(json));
esp_now_secure_send((uint8_t*)json, len);

// Informations sur un canal
const char *name, *unit;
sensor_manager_get_channel_info(SENSOR_TYPE_ADS7128, 0, &name, &unit);
// name = "Temp_Moteur", unit = "°C"
```

### Tâches créées automatiquement

Le sensor_manager crée **3 tâches FreeRTOS** (une par type d'ADC) :
- `ads7128_acq` : Acquisition des 8 canaux toutes les 5s
- `ads1119_1_acq` : Acquisition des 4 canaux toutes les 5s
- `ads1119_2_acq` : Acquisition des 4 canaux toutes les 5s

**Priorité** : 6 (haute) pour garantir précision temporelle

**Avantages** :
- ✅ Code métier simplifié (pas de gestion manuelle des tâches ADC)
- ✅ Données toujours à jour en arrière-plan
- ✅ Thread-safe (mutex intégrés)
- ✅ Conversions appliquées automatiquement

## 📝 Main.c simplifié

Avant (ancien main.c) : **~570 lignes** avec duplication de code

Après (main_new.c) : **~280 lignes**, logique claire

### Structure du nouveau main.c

```c
void app_main(void) {
    // 1. Initialisation système
    nvs_flash_init();
    i2c_bus_init(...);
    
    // 2. Init périphériques GPIO
    tca9537_init(...);
    
    // 3. Init sensor_manager → Acquisition automatique !
    sensor_manager_init(&config);
    
    // 4. Init ESP-NOW sécurisé
    esp_now_secure_init(&config);
    
    // 5. Init Bluetooth scanner
    bt_spp_init(&config);
    
    // 6. Créer tâches applicatives seulement
    xTaskCreate(heartbeat_task, ...);
    xTaskCreate(esp_now_tx_task, ...);  // Si MODE_NODE
    
    // FIN - Les acquisitions tournent en arrière-plan
}
```

**Ce qui a disparu** :
- ❌ 9 tâches FreeRTOS manuelles (acq + log pour chaque ADC)
- ❌ 3 queues manuelles
- ❌ Code dupliqué pour chaque ADC
- ❌ Conversions manuelles répétées

**Ce qui reste** :
- ✅ Callbacks simples (ESP-NOW, Bluetooth, capteurs)
- ✅ Tâche heartbeat
- ✅ Tâche transmission ESP-NOW (mode nœud)
- ✅ Logique métier uniquement

## 🚀 Workflow de développement

### Ajouter un nouveau nœud

1. **Dupliquer le projet** ou utiliser Git branch
2. **Modifier `node_config.h`** :
   ```c
   #define NODE_ID         3
   #define NODE_NAME       "APRU40-Node-03"
   #define ENABLE_ADS1119_2    0  // Pas de 2ème ADS1119 sur ce nœud
   ```
3. **Compiler et flasher** → C'est tout !

### Ajouter un nouveau type de capteur

1. **Calculer conversion** (datasheet capteur)
   - Exemple : Capteur humidité 0-100% = 0-5V
   - Conversion : `y = 100.0 / 4095.0 * x`

2. **Modifier `conversion_config.h`** :
   ```c
   {
       .name = "Humidite_Air",
       .unit = "%",
       .type = CONV_LINEAR,
       .linear = {.a = 100.0/4095.0, .b = 0.0}
   }
   ```

3. **Recompiler** → Le sensor_manager applique automatiquement

### Changer de mode nœud → gateway

1. **Modifier `node_config.h`** :
   ```c
   #define NODE_MODE       MODE_GATEWAY
   #define NODE_ID         255
   #define ENABLE_ADS7128  0  // Gateway n'a pas de capteurs
   ```

2. **Recompiler** → Le code `#if NODE_MODE == MODE_GATEWAY` s'active automatiquement

## 📊 Exemple de flux de données

### Mode NŒUD

```
[Capteur physique]
      ↓
[ADC brut] ←─── acquisition automatique (sensor_manager task)
      ↓
[Conversion] ←── conversion_config.h (linéaire/polynôme/LUT)
      ↓
[Valeur physique] ←─ stockée en mémoire (thread-safe)
      ↓
[Format JSON] ←── sensor_manager_format_json()
      ↓
[Chiffrement AES+HMAC] ←── esp_now_secure_send()
      ↓
[Transmission WiFi] ──► Gateway
```

### Mode GATEWAY

```
[Réception WiFi] ←── ESP-NOW chiffré
      ↓
[Déchiffrement] ←── esp_now_secure (vérification HMAC)
      ↓
[Callback RX] ←── esp_now_recv_callback()
      ↓
[Parse JSON] ←── extraire node_id, valeurs, timestamps
      ↓
[Traitement] ←── logs, stockage, envoi MQTT, etc.
```

## 🎯 Avantages de cette architecture

### Pour le développement

| Avant | Après |
|-------|-------|
| Modifier main.c pour chaque nœud | Modifier uniquement node_config.h |
| Duplication de code (3× ADC) | Factorisation dans sensor_manager |
| Conversions éparpillées | Centralisées dans conversion_config.h |
| 9 tâches FreeRTOS manuelles | 3 tâches automatiques + 2 applicatives |
| ~570 lignes main.c | ~280 lignes main.c |

### Pour la maintenance

- ✅ **Ajout capteur** : Modifier uniquement `conversion_config.h`
- ✅ **Debug** : Logs automatiques avec noms de canaux
- ✅ **Scalabilité** : Facile d'ajouter 10 nœuds (juste NODE_ID)
- ✅ **Test** : Mode simulation (`SIMULATION_MODE`) intégré

### Pour la production

- ✅ **Déploiement rapide** : Flasher avec configuration spécifique
- ✅ **Traçabilité** : NODE_ID + NODE_NAME dans tous les messages
- ✅ **Flexibilité** : Activer/désactiver périphériques sans recompiler code métier

## 📖 Exemples d'utilisation

### Exemple 1 : Nœud avec 3 ADC + scanner

```c
// node_config.h
#define NODE_MODE           MODE_NODE
#define NODE_ID             1
#define ENABLE_ADS7128      1
#define ENABLE_ADS1119_1    1
#define ENABLE_ADS1119_2    1
#define ENABLE_BLUETOOTH_SPP 1
```

**Résultat** : 
- Acquisition automatique toutes les 5s
- Envoi JSON via ESP-NOW toutes les 10s
- Scanner Bluetooth opérationnel

### Exemple 2 : Gateway simple

```c
// node_config.h
#define NODE_MODE           MODE_GATEWAY
#define NODE_ID             255
#define ENABLE_ADS7128      0  // Pas de capteurs
#define ENABLE_ADS1119_1    0
#define ENABLE_ADS1119_2    0
#define ENABLE_BLUETOOTH_SPP 0
```

**Résultat** :
- Réception ESP-NOW uniquement
- Traitement des données des nœuds
- Pas d'acquisition ADC

### Exemple 3 : Nœud spécialisé (1 seul ADC)

```c
// node_config.h
#define NODE_MODE           MODE_NODE
#define NODE_ID             5
#define ENABLE_ADS7128      1  // Seulement celui-ci
#define ENABLE_ADS1119_1    0
#define ENABLE_ADS1119_2    0
#define ENABLE_BLUETOOTH_SPP 0
```

**Résultat** :
- Acquisition ADS7128 uniquement
- Taille firmware réduite (moins de composants compilés)
- Consommation réduite

## 🔧 Intégration avec le code existant

Pour migrer de l'ancien main.c vers la nouvelle architecture :

1. **Copier** `main_new.c` → `main.c`
2. **Vérifier** que `node_config.h` correspond à votre besoin
3. **Adapter** `conversion_config.h` avec vos capteurs réels
4. **Compiler** et flasher

Les anciens fichiers restent disponibles pour référence.

## 📚 Fichiers de l'architecture

```
APRU40/
├── include/
│   ├── node_config.h          ← Configuration centralisée
│   └── conversion_config.h    ← Lois de conversion
├── components/
│   ├── sensor_manager/        ← Nouveau composant
│   │   ├── sensor_manager.h
│   │   ├── sensor_manager.c
│   │   └── CMakeLists.txt
│   ├── esp_now_secure/
│   ├── bluetooth_spp/
│   ├── ads7128/
│   ├── ads1119/
│   ├── tca9537/
│   └── i2c_bus/
├── main/
│   ├── main.c                 ← Ancien (à remplacer)
│   └── main_new.c             ← Nouveau (simplifié)
└── README.md
```

---

**Cette architecture permet de gérer 30+ nœuds avec configurations différentes en modifiant uniquement 2 fichiers `.h` par nœud, sans toucher au code métier.**
