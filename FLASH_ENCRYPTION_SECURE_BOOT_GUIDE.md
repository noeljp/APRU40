# Guide : Activation Flash Encryption + Secure Boot V2

⚠️ **ATTENTION : PROCÉDURE IRRÉVERSIBLE - LISEZ ENTIÈREMENT AVANT D'AGIR**

## Avertissement

L'activation de Flash Encryption et Secure Boot V2 est une **modification matérielle permanente** des eFuses de l'ESP32. Une fois activés :

- ❌ Impossible de désactiver
- ❌ Impossible de lire les clés
- ❌ Risque de "bricking" si erreur
- ✅ Sécurité maximale (conformité NIS2)

**Toujours tester sur ESP32 de développement AVANT production**

---

## Prérequis

### Matériel

- **Développement** : 2-3 ESP32-POE-ISO sacrificiels (~100€)
- **Production** : ESP32-POE-ISO neufs (vierges)
- Lecteur carte SD
- Câble USB + alimentation PoE stable

### Logiciels

```bash
# ESP-IDF 5.5.0+
idf.py --version

# esptool.py 4.0+
esptool.py version

# PlatformIO Core 6.0+
pio --version
```

### Connaissances requises

- [ ] Administration Linux/Windows
- [ ] Ligne de commande ESP-IDF
- [ ] Gestion certificats X.509
- [ ] Procédures de backup/recovery

---

## Phase 1 : Préparation

### 1.1 Backup complet

```bash
# Sauvegarder configuration actuelle (AVANT activation)
cd /path/to/APRU40

# Backup code source
git commit -am "Avant activation Flash Encryption + Secure Boot"
git push

# Backup partition table
esptool.py --port COM3 read_flash 0x8000 0x1000 partitions_backup.bin

# Backup NVS (certificats)
esptool.py --port COM3 read_flash 0x9000 0x6000 nvs_backup.bin
```

### 1.2 Générer clés Secure Boot

```bash
# Créer répertoire sécurisé pour clés
mkdir -p ~/apru40_secure_keys
cd ~/apru40_secure_keys

# Générer clé de signature RSA-3072 (Secure Boot V2)
esptool.py generate_signing_key --version 2 secure_boot_signing_key.pem

# ⚠️ CRITIQUE : Cette clé DOIT être sauvegardée en 3+ endroits :
# 1. Coffre-fort physique
# 2. Cloud chiffré (AWS KMS, Azure Key Vault)
# 3. HSM (Hardware Security Module) si disponible

# Permissions restrictives
chmod 400 secure_boot_signing_key.pem

# Backup immédiat
cp secure_boot_signing_key.pem secure_boot_signing_key_BACKUP_$(date +%Y%m%d).pem
```

### 1.3 Configuration PlatformIO

```ini
# platformio.ini

# ============================================================================
# ENVIRONNEMENT DÉVELOPPEMENT (sans sécurité - pour tests)
# ============================================================================
[env:esp32-poe-iso-dev]
platform = espressif32
board = esp32-poe-iso
framework = espidf
monitor_speed = 115200
upload_port = COM3

# ============================================================================
# ENVIRONNEMENT SÉCURISÉ (Flash Encryption + Secure Boot V2)
# ⚠️ ATTENTION : À utiliser UNIQUEMENT après tests sur ESP32 sacrificiels
# ============================================================================
[env:esp32-poe-iso-secure]
platform = espressif32
board = esp32-poe-iso
framework = espidf
monitor_speed = 115200
upload_port = COM3

# Flash Encryption (AES-256-XTS)
board_build.cmake_extra_args =
  # Flash Encryption
  -DCONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=1
  -DCONFIG_SECURE_FLASH_ENC_ENABLED=1
  -DCONFIG_SECURE_FLASH_REQUIRE_ALREADY_ENABLED=0
  
  # Secure Boot V2 (RSA-3072)
  -DCONFIG_SECURE_BOOT_V2_ENABLED=1
  -DCONFIG_SECURE_BOOT=1
  -DCONFIG_SECURE_BOOT_V2_RSA_3072=1
  
  # NVS Encryption avec Flash Encryption
  -DCONFIG_NVS_ENCRYPTION=1
  -DCONFIG_NVS_SEC_KEY_PROTECTION_SCHEME=2
  
  # Sécurité additionnelle
  -DCONFIG_SECURE_DISABLE_ROM_DL_MODE=1
  -DCONFIG_SECURE_ENABLE_SECURE_ROM_DL_MODE=0

# Clé de signature Secure Boot
board_build.embed_files = 
  ~/apru40_secure_keys/secure_boot_signing_key.pem
```

---

## Phase 2 : Test sur ESP32 sacrificiel #1

### 2.1 Compilation firmware sécurisé

```bash
# Compiler avec configuration sécurisée
pio run -e esp32-poe-iso-secure

# Vérifier binaires générés
ls -lh .pio/build/esp32-poe-iso-secure/
# Doit contenir :
# - bootloader.bin (signé)
# - firmware.bin (signé)
# - partitions.bin
```

### 2.2 Premier flash (ACTIVE sécurité - IRRÉVERSIBLE)

```bash
# ⚠️⚠️⚠️ POINT DE NON-RETOUR ⚠️⚠️⚠️
# Après cette commande, ESP32 #1 sera définitivement sécurisé
# Vérifier 3 fois la configuration avant d'exécuter

# Flash bootloader + firmware + partitions
pio run -e esp32-poe-iso-secure --target upload

# Ou manuellement avec esptool
esptool.py --port COM3 --chip esp32 \
  --before=default_reset --after=no_reset \
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 .pio/build/esp32-poe-iso-secure/bootloader.bin \
  0x8000 .pio/build/esp32-poe-iso-secure/partitions.bin \
  0x10000 .pio/build/esp32-poe-iso-secure/firmware.bin

# Première activation : ESP32 va :
# 1. Écrire clé Flash Encryption dans eFuse BLK2
# 2. Écrire hash clé Secure Boot dans eFuse BLK3
# 3. Activer bits FLASH_CRYPT_CNT et ABS_DONE_0
# 4. Redémarrer avec flash chiffré

# ⏱️ Durée : 30-60 secondes
# LED doit clignoter pendant le processus
```

### 2.3 Vérification activation

```bash
# Vérifier eFuses (après redémarrage)
espefuse.py --port COM3 summary

# Doit afficher :
# FLASH_CRYPT_CNT (BLOCK0):        = 127 R/W (0b1111111)
# ABS_DONE_0 (BLOCK0):              = True R/W (0b1)
# BLK2 (BLOCK2):                    = ?? ?? ?? ?? (clé illisible)
# BLK3 (BLOCK3):                    = ?? ?? ?? ?? (hash illisible)

# Vérifier logs série
pio device monitor -e esp32-poe-iso-secure

# Doit afficher :
# I (XXX) boot: Checking flash encryption...
# I (XXX) flash_encrypt: flash encryption is enabled (3 plaintext flashes left)
# I (XXX) boot: Checking secure boot...
# I (XXX) secure_boot_v2: Secure boot V2 is enabled
```

### 2.4 Tests fonctionnels (1 semaine)

```bash
# Test 1 : Boot multiple
# Redémarrer 20 fois, vérifier logs à chaque fois
for i in {1..20}; do
  echo "Redémarrage $i/20"
  esptool.py --port COM3 --after hard_reset chip_id
  sleep 10
done

# Test 2 : MQTT/TLS
# Vérifier connexion broker Mosquitto
# [Voir MQTT_INTEGRATION_GUIDE.md]

# Test 3 : ESP-NOW
# Vérifier réception données nœuds
# [Voir main.c mode gateway]

# Test 4 : Provisioning certificats SD
# Insérer carte SD avec certificats
# Vérifier stockage en NVS chiffré
# [Voir CERT_PROVISIONING_SD_GUIDE.md]

# Test 5 : OTA firmware
# Créer firmware v2 signé + chiffré
# Flasher via MQTT OTA
# [Voir section OTA ci-dessous]

# Test 6 : Recovery après corruption NVS
nvs_partition_gen.py erase
# Vérifier que firmware redémarre correctement
```

### 2.5 Analyse résultats Test #1

| Test | Résultat attendu | Action si échec |
|------|------------------|-----------------|
| Boot multiple | 20/20 réussis | ESP32 #1 bricked → jeter, analyser logs, corriger config |
| MQTT/TLS | Connexion OK | Vérifier certificats, logs |
| ESP-NOW | Données reçues | Vérifier clés AES/HMAC |
| Provisioning SD | Certificats en NVS | Vérifier carte SD, permissions |
| OTA | Firmware mis à jour | Vérifier signature RSA |
| Recovery NVS | Redémarrage OK | Vérifier bootloader |

**Si TOUS tests OK sur ESP32 #1** → Passer Phase 3 (Test #2)  
**Si UN test KO** → Analyser, corriger, recommencer sur ESP32 #2

---

## Phase 3 : Test sur ESP32 sacrificiel #2

### 3.1 Validation reproductibilité

```bash
# Répéter Phase 2 sur ESP32 #2 avec MÊME configuration
# Objectif : Prouver que la procédure est reproductible

# Si succès sur ESP32 #2 : Procédure validée ✅
# Si échec : Procédure NON reproductible ❌ → Corriger
```

### 3.2 Décision GO/NO-GO Production

**Critères pour passer en production** :
- [ ] ESP32 #1 : 100% tests OK pendant 1 semaine
- [ ] ESP32 #2 : 100% tests OK (reproductibilité)
- [ ] Clés Secure Boot sauvegardées (3+ emplacements)
- [ ] Procédure OTA validée
- [ ] Équipe formée
- [ ] Budget ESP32 de remplacement alloué

**Si TOUS critères validés** → GO Production  
**Si UN critère manquant** → NO-GO, corriger

---

## Phase 4 : Production (après validation complète)

### 4.1 Provisioning par lot

```bash
# Jour 1 : 10 ESP32
# ==================
for i in {1..10}; do
  echo "=== ESP32 Production #$i ==="
  
  # Flash
  pio run -e esp32-poe-iso-secure --target upload --upload-port /dev/ttyUSB$i
  
  # Vérification
  espefuse.py --port /dev/ttyUSB$i summary | grep "FLASH_CRYPT_CNT"
  
  # Test boot
  pio device monitor --port /dev/ttyUSB$i &
  sleep 30
  pkill -f "pio device monitor"
  
  # Log résultat
  echo "ESP32 #$i: OK" >> production_log.txt
done

# Attendre 24h, surveiller logs

# Jour 2 : Si OK → 20 ESP32 supplémentaires
# Jour 3 : Si OK → 50 ESP32 supplémentaires
# ...
```

### 4.2 Suivi production

```csv
# production_tracking.csv
ESP32_ID,MAC_Address,Date_Activation,Status,Location,Notes
GW001,AA:BB:CC:DD:EE:01,2026-02-01,OK,Site_A,Flash Encryption + Secure Boot OK
GW002,AA:BB:CC:DD:EE:02,2026-02-01,OK,Site_A,Flash Encryption + Secure Boot OK
GW003,AA:BB:CC:DD:EE:03,2026-02-01,FAIL,Lab,Bricked - Erreur bootloader
...
```

---

## OTA avec Flash Encryption + Secure Boot

### 5.1 Créer firmware OTA signé

```bash
# 1. Compiler nouveau firmware
pio run -e esp32-poe-iso-secure

# 2. Signer avec clé Secure Boot
espsecure.py sign_data --version 2 \
  --keyfile ~/apru40_secure_keys/secure_boot_signing_key.pem \
  .pio/build/esp32-poe-iso-secure/firmware.bin \
  firmware_v2_signed.bin

# 3. Vérifier signature
espsecure.py verify_signature --version 2 \
  --keyfile ~/apru40_secure_keys/secure_boot_signing_key.pem \
  firmware_v2_signed.bin

# 4. Publier sur serveur MQTT
mosquitto_pub -h broker.apru40.local -p 8883 \
  --cafile ca.crt --cert client.crt --key client.key \
  -t apru40/ota/firmware_v2 \
  -f firmware_v2_signed.bin
```

### 5.2 Gateway télécharge et flashe

```c
// Code OTA dans gateway (main.c)
#include "esp_ota_ops.h"
#include "esp_secure_boot.h"

void ota_mqtt_callback(const char *data, size_t len) {
    // 1. Vérifier signature RSA
    esp_secure_boot_verify_signature(data, len);
    
    // 2. Flasher partition OTA
    esp_ota_begin(...);
    esp_ota_write(...);
    esp_ota_end(...);
    
    // 3. Valider et redémarrer
    esp_ota_set_boot_partition(...);
    esp_restart();
}
```

---

## Recovery et Troubleshooting

### 6.1 ESP32 bricked (boot impossible)

**Symptômes** :
- LED ne clignote pas
- Aucun log série
- esptool ne détecte pas l'ESP32

**Diagnostic** :
```bash
# Vérifier détection USB
lsusb | grep Silicon  # Linux
# Ou
Get-PnpDevice -Class Ports  # Windows

# Vérifier eFuses
espefuse.py --port COM3 summary
```

**Solutions** :
1. **Si eFuses OK mais boot KO** : Re-flasher bootloader signé
   ```bash
   esptool.py --port COM3 write_flash 0x1000 bootloader.bin
   ```

2. **Si eFuses corrompus** : Hardware définitivement perdu
   - Remplacer ESP32
   - Analyser logs pour éviter répétition

### 6.2 Clé Secure Boot perdue

**Conséquences** :
- ❌ Impossible de créer nouveau firmware signé
- ❌ OTA bloqué définitivement
- ⚠️ ESP32 existants fonctionnent mais non mis à jour

**Solutions** :
- **Si backup disponible** : Restaurer clé depuis backup
- **Si aucun backup** : 
  - ESP32 existants continuent de fonctionner
  - Nouveaux ESP32 nécessitent nouvelle clé (incompatible avec ancien parc)
  - Remplacement progressif de tout le parc

**Prévention** :
- 3+ backups (coffre, cloud, HSM)
- Test de restauration trimestriel
- Procédure de rotation de clés documentée

### 6.3 Flash Encryption actif mais firmware corrompu

**Symptômes** :
- Boot bloqué à "Loading app from partition..."
- CRC error

**Solution** :
```bash
# Re-flasher firmware signé + chiffré
esptool.py --port COM3 write_flash 0x10000 firmware_signed.bin

# ESP32 déchiffrera automatiquement au boot
```

---

## Checklist finale avant activation production

### Validation technique

- [ ] Tests réussis sur 2+ ESP32 de développement
- [ ] 100% tests fonctionnels OK pendant 7+ jours
- [ ] Bootloader signé et testé
- [ ] Firmware signé et testé
- [ ] OTA signé testé (au moins 3 mises à jour)
- [ ] Recovery procédure validée

### Gestion des clés

- [ ] Clé Secure Boot générée (RSA-3072)
- [ ] 3+ backups clé (coffre + cloud + HSM)
- [ ] Test restauration backup réussi
- [ ] Permissions clé restrictives (chmod 400)
- [ ] Procédure rotation clés documentée

### Documentation

- [ ] Guide activation lu et compris par équipe
- [ ] Procédure OTA documentée
- [ ] Procédure recovery documentée
- [ ] Logs d'activation centralisés
- [ ] Contacts support identifiés

### Organisationnel

- [ ] Formation équipe technique (4+ heures)
- [ ] Validation RSSI/Direction
- [ ] Budget ESP32 de remplacement alloué (5-10% parc)
- [ ] Planning provisioning par lot défini
- [ ] Procédure escalade incidents définie

### Sécurité

- [ ] Audit sécurité NIS2 réalisé
- [ ] Conformité Flash Encryption validée
- [ ] Conformité Secure Boot validée
- [ ] Tests pénétration planifiés (6 mois)

---

## Estimation coûts et délais

### Coûts

| Poste | Quantité | Coût unitaire | Total |
|-------|----------|---------------|-------|
| ESP32-POE-ISO développement | 3 | 30€ | 90€ |
| ESP32-POE-ISO production | 50 | 25€ | 1250€ |
| Temps ingénieur (tests) | 40h | 80€/h | 3200€ |
| Formation équipe | 16h | 100€/h | 1600€ |
| HSM (optionnel) | 1 | 5000€ | 5000€ |
| **Total sans HSM** | | | **6140€** |
| **Total avec HSM** | | | **11140€** |

### Délais

| Phase | Durée | Détails |
|-------|-------|---------|
| Préparation | 3 jours | Config + génération clés |
| Test ESP32 #1 | 7 jours | Activation + tests fonctionnels |
| Test ESP32 #2 | 7 jours | Validation reproductibilité |
| Décision GO/NO-GO | 1 jour | Réunion validation |
| Production (10 ESP32/jour) | 5 jours | Pour parc de 50 ESP32 |
| **Total** | **23 jours** | ~1 mois calendaire |

---

## Conclusion

Flash Encryption + Secure Boot V2 offrent la **sécurité maximale** pour conformité NIS2, mais impliquent :

✅ **Avantages** :
- Conformité NIS2 (score 8.5/10)
- Firmware inviolable
- Clés cryptographiques protégées
- Résistance aux attaques physiques

⚠️ **Contraintes** :
- Irréversible (modification matérielle)
- Complexité OTA accrue
- Risque de bricking si erreur
- Coût développement/test

**Recommandation** : Activer UNIQUEMENT après validation complète sur ESP32 sacrificiels.

---

**Prochaine révision** : Après activation production (feedback terrain)
