# Guide de Configuration Dynamique et OTA

## 📡 Principe de fonctionnement

Au lieu de recompiler et reflasher chaque module, vous pouvez **modifier la configuration à distance** via ESP-NOW ou mettre à jour le firmware complet via OTA.

```
┌─────────────────────────────────────────────────────────────────┐
│                    STRATÉGIES DE MISE À JOUR                    │
└─────────────────────────────────────────────────────────────────┘

1️⃣  CONFIGURATION DYNAMIQUE (Recommandé pour paramètres)
   ┌──────────┐  ESP-NOW  ┌──────────┐    NVS    ┌──────────┐
   │ Gateway  │ ────────► │  Nœud 5  │ ────────► │  Flash   │
   │          │  Commande │          │ Persiste  │ (128KB)  │
   └──────────┘           └──────────┘           └──────────┘
   
   Exemple : Changer node_id=5, tx_period=20s
   → Pas besoin de recompiler
   → Effet immédiat ou après reboot
   → Survit aux coupures de courant

2️⃣  OTA FIRMWARE (Pour changements de code)
   ┌──────────┐    HTTP    ┌──────────┐  Écriture  ┌──────────┐
   │ Serveur  │ ─────────► │  Nœud 5  │ ─────────► │ OTA Part │
   │ firmware │   .bin     │          │  firmware  │ (1.5MB)  │
   └──────────┘            └──────────┘            └──────────┘
   
   Exemple : Nouveau driver, correction bug
   → Nécessite compilation sur PC
   → Téléchargement via WiFi
   → Redémarrage automatique
```

## 🔧 Option 1 : Configuration Dynamique (NVS + ESP-NOW)

### Architecture

```
Boot Time                 Runtime                  Shutdown
──────────────────────────────────────────────────────────────
│                          │                        │
├─ config_manager_init()  │                        │
│  ├─ Lire NVS           │                        │
│  ├─ Si vide → défauts  │                        │
│  └─ Charger en RAM     │                        │
│                          │                        │
│                          ├─ ESP-NOW RX          │
│                          │  "SET_NODE_ID=10"    │
│                          │                        │
│                          ├─ process_command()   │
│                          │  Modifier config RAM │
│                          │                        │
│                          ├─ config_manager_save()│
│                          │  Écrire NVS          │
│                          │                        │
│                          │                        ├─ Reboot
│                          │                        └─ Config
                                                       persistée
```

### Exemple 1 : Changer le node_id à distance

**Depuis le gateway** (ou un PC avec ESP-NOW) :

```c
// Créer la commande
config_cmd_packet_t cmd;
uint8_t new_id = 10;
config_manager_create_command(&cmd, CONFIG_CMD_SET_NODE_ID, 
                               5,        // Nœud cible (ID=5)
                               &new_id, 1);

// Envoyer via ESP-NOW sécurisé
esp_now_secure_send((uint8_t*)&cmd, sizeof(cmd));
```

**Sur le nœud cible** (automatique) :

```c
// Dans le callback ESP-NOW
void esp_now_recv_callback(const uint8_t *mac, const uint8_t *data, uint8_t len, int8_t rssi) {
    // Détecter si c'est une commande de config
    if (len >= sizeof(config_cmd_packet_t)) {
        config_cmd_packet_t *cmd = (config_cmd_packet_t*)data;
        
        if (cmd->target_node_id == NODE_ID || cmd->target_node_id == 0xFF) {
            // Traiter la commande
            config_manager_process_command(cmd);
            // → node_id changé et sauvegardé en NVS
            // → Prendra effet au prochain reboot
        }
    }
}
```

### Exemple 2 : Changer plusieurs paramètres

**Modifier période d'acquisition** :

```c
// Gateway envoie commande
config_cmd_packet_t cmd;
uint32_t new_period = 10000;  // 10 secondes
config_manager_create_command(&cmd, CONFIG_CMD_SET_PERIOD,
                               0xFF,  // Tous les nœuds
                               (uint8_t*)&new_period, sizeof(new_period));
esp_now_secure_send((uint8_t*)&cmd, sizeof(cmd));
```

**Activer/désactiver un périphérique** :

```c
// Désactiver ADS1119_2 sur nœud 3
struct {
    uint8_t device_id;    // 0=ADS7128, 1=ADS1119_1, 2=ADS1119_2, etc.
    bool enable;
} device_cmd = {2, false};

config_manager_create_command(&cmd, CONFIG_CMD_ENABLE_DEVICE,
                               3, (uint8_t*)&device_cmd, sizeof(device_cmd));
esp_now_secure_send((uint8_t*)&cmd, sizeof(cmd));
```

### Exemple 3 : Obtenir la config actuelle d'un nœud

```c
// Gateway demande config du nœud 5
config_cmd_packet_t cmd;
config_manager_create_command(&cmd, CONFIG_CMD_GET_CONFIG, 5, NULL, 0);
esp_now_secure_send((uint8_t*)&cmd, sizeof(cmd));

// Le nœud 5 répond avec un JSON
// → Reçu dans esp_now_recv_callback() du gateway
// {"node_id":5,"mode":1,"adc_period":5000,...}
```

## 🚀 Option 2 : OTA Firmware Update

### Principe

Le firmware `.bin` compilé est téléchargé via HTTP/HTTPS et écrit dans une partition OTA de la flash. Au reboot, le bootloader démarre depuis la nouvelle partition.

### Architecture de partitions

```
Flash 4MB
┌─────────────────────────────────────────────┐
│ 0x00000  Bootloader        (32 KB)         │
├─────────────────────────────────────────────┤
│ 0x08000  Partition Table   (4 KB)          │
├─────────────────────────────────────────────┤
│ 0x09000  NVS               (24 KB)         │  ← Config ici
├─────────────────────────────────────────────┤
│ 0x10000  OTA_0 (Factory)   (1.5 MB)        │  ← Firmware actuel
├─────────────────────────────────────────────┤
│ 0x190000 OTA_1 (Update)    (1.5 MB)        │  ← Nouveau firmware
├─────────────────────────────────────────────┤
│ 0x310000 OTA Data          (8 KB)          │  ← Quelle partition boot?
└─────────────────────────────────────────────┘
```

### Table de partitions pour OTA

Créer `partitions_ota.csv` :

```csv
# Name,   Type, SubType,  Offset,   Size,    Flags
nvs,      data, nvs,      0x9000,   0x6000,
phy_init, data, phy,      0xf000,   0x1000,
otadata,  data, ota,      0x10000,  0x2000,
ota_0,    app,  ota_0,    0x20000,  0x180000,
ota_1,    app,  ota_1,    0x1A0000, 0x180000,
```

Modifier `platformio.ini` :

```ini
[env:esp32-poe-iso]
board_build.partitions = partitions_ota.csv
```

### Exemple : Démarrer OTA depuis le nœud

**Méthode 1 : Via commande ESP-NOW**

```c
// Gateway envoie commande OTA au nœud 5
const char *firmware_url = "http://192.168.1.100:8000/firmware_v2.bin";

config_cmd_packet_t cmd;
config_manager_create_command(&cmd, CONFIG_CMD_OTA_START, 5,
                               (uint8_t*)firmware_url, strlen(firmware_url));
esp_now_secure_send((uint8_t*)&cmd, sizeof(cmd));
```

**Méthode 2 : Depuis le nœud lui-même**

```c
// Bouton poussoir détecté → lancer OTA
const char *url = "http://192.168.1.100:8000/firmware.bin";
config_manager_ota_start(url);

// Monitorer progression
uint8_t progress;
const char *status;
while (config_manager_ota_get_status(&progress, &status)) {
    ESP_LOGI(TAG, "OTA: %d%% - %s", progress, status);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// OTA terminée → reboot automatique
```

### Serveur HTTP simple pour OTA

Sur votre PC (Python) :

```bash
# Placer le firmware.bin dans un dossier
cd /chemin/vers/firmware
python -m http.server 8000

# Le nœud télécharge depuis http://192.168.1.100:8000/firmware.bin
```

## 📊 Comparaison des méthodes

| Critère | Config Dynamique (NVS) | OTA Firmware | Reflash USB |
|---------|------------------------|--------------|-------------|
| **Cas d'usage** | Paramètres (ID, périodes, clés) | Nouveau code, drivers | Développement |
| **Vitesse** | ⚡ Instantané | 🐢 1-2 min (téléchargement) | 🐌 30s + accès physique |
| **Downtime** | 0s (ou reboot 5s) | Reboot 10s | 30s |
| **Connexion physique** | ❌ Non | ❌ Non | ✅ Oui (USB) |
| **Taille données** | <1 KB | 1-1.5 MB | 1-1.5 MB |
| **Portée** | 200m (ESP-NOW) | Illimitée (WiFi) | 0m (câble) |
| **Rollback** | ✅ config_manager_reset() | ✅ Partition précédente | ⚠️ Reflash |
| **Sécurité** | ⚠️ Vérifier HMAC ESP-NOW | ✅ HTTPS + signature | ✅ USB sécurisé |

## 🎯 Workflow recommandé

### Déploiement initial (30 nœuds)

1. **Compiler firmware avec config par défaut** :
   ```bash
   pio run --environment esp32-poe-iso
   ```

2. **Flasher tous les modules** (USB, une fois) :
   ```bash
   pio run -t upload
   ```

3. **Configurer chaque nœud à distance** :
   ```c
   // Depuis le gateway
   for (int id = 1; id <= 30; id++) {
       config_cmd_packet_t cmd;
       config_manager_create_command(&cmd, CONFIG_CMD_SET_NODE_ID, 
                                      0xFF, &id, 1);
       esp_now_secure_send(...);
       vTaskDelay(pdMS_TO_TICKS(100));
   }
   ```

### Modification en production

**Changement de paramètre** (ex: période) :
→ **Config dynamique** (ESP-NOW)

**Correction de bug** :
→ **OTA** via WiFi (pas besoin d'accès physique)

**Ajout de fonctionnalité majeure** :
→ **OTA** si compatible, sinon reflash USB

## 🔒 Sécurité

### Configuration dynamique

```c
// Vérifier que la commande vient d'une source de confiance
void esp_now_recv_callback(const uint8_t *mac, ...) {
    // 1. Vérifier HMAC (déjà fait par esp_now_secure)
    // 2. Vérifier whitelist MAC
    if (!is_mac_authorized(mac)) {
        ESP_LOGW(TAG, "Commande config rejetée (MAC non autorisé)");
        return;
    }
    
    // 3. Traiter commande
    config_manager_process_command(...);
}
```

### OTA firmware

```c
// Utiliser HTTPS au lieu de HTTP
const char *url = "https://serveur.com/firmware.bin";

// Vérifier signature du firmware
esp_https_ota_config_t ota_config = {
    .url = url,
    .cert_pem = server_cert_pem,  // Certificat serveur
};
```

## 📝 Exemples pratiques

### Cas 1 : Augmenter la période d'acquisition (économiser batterie)

```c
// Gateway envoie à tous les nœuds
config_cmd_packet_t cmd;
uint32_t new_period = 30000;  // 30 secondes au lieu de 5
config_manager_create_command(&cmd, CONFIG_CMD_SET_PERIOD,
                               0xFF, (uint8_t*)&new_period, 4);
esp_now_secure_send((uint8_t*)&cmd, sizeof(cmd));
```

### Cas 2 : Changer clé de chiffrement ESP-NOW

```c
// Nouvelle clé AES-256
uint8_t new_key[32] = {0x12, 0x34, ...};

// Envoyer via ESP-NOW ancien (chiffré avec ancienne clé)
config_cmd_packet_t cmd = {
    .cmd_type = CONFIG_CMD_SET_AES_KEY,
    .target_node_id = 0xFF,
    .data_len = 32
};
memcpy(cmd.data, new_key, 32);

esp_now_secure_send((uint8_t*)&cmd, sizeof(cmd));

// Tous les nœuds switchent vers nouvelle clé
// Prochains messages utilisent nouvelle clé
```

### Cas 3 : Désactiver Bluetooth sur nœuds de terrain

```c
// Économiser RAM/CPU sur nœuds éloignés (pas besoin de scanner)
struct {
    uint8_t device_id;  // 4 = Bluetooth
    bool enable;
} disable_bt = {4, false};

config_manager_create_command(&cmd, CONFIG_CMD_ENABLE_DEVICE,
                               0xFF, (uint8_t*)&disable_bt, 2);
esp_now_secure_send(...);

// Tous les nœuds désactivent Bluetooth
// Libère ~100 KB RAM
```

## 🛠️ Outils de gestion

### Script Python pour envoyer commandes

```python
#!/usr/bin/env python3
import espnow
import struct

# Initialiser ESP-NOW depuis PC (avec dongle ESP32)
e = espnow.ESPNow()

# Changer node_id du nœud 5
cmd = struct.pack('<BBH200s', 
                  0x01,        # CONFIG_CMD_SET_NODE_ID
                  5,           # Target node
                  1,           # Data len
                  b'\x0A')     # Nouveau ID = 10

e.send(cmd)
print("Commande envoyée au nœud 5")
```

### Interface Web (sur gateway)

Créer une page HTML sur le gateway pour gérer les nœuds :

```
┌─────────────────────────────────────────┐
│       APRU40 Configuration Manager      │
├─────────────────────────────────────────┤
│ Nœud 1:  [ID: 1 ]  [Période: 5000ms ▼] │
│          [✓] ADS7128  [✓] Bluetooth     │
│          [Sauvegarder] [Redémarrer]     │
├─────────────────────────────────────────┤
│ Nœud 2:  [ID: 2 ]  [Période: 10000ms▼] │
│          ...                            │
└─────────────────────────────────────────┘
```

---

**Résumé** : Pour modifier la configuration sans reflasher, utilisez le **config_manager** avec NVS + ESP-NOW. Pour mettre à jour le code, utilisez l'**OTA**. Les deux méthodes peuvent être combinées pour une gestion complète à distance de votre réseau de 30 nœuds.
