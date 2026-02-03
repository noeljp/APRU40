# Guide : Provisioning Certificats TLS via Carte SD

## Vue d'ensemble

Cette méthode permet de provisionner de manière **sécurisée** les certificats TLS nécessaires à la connexion MQTT des passerelles APRU40.

### Avantages

- ✅ **Sécurité maximale** : Certificats jamais transmis par réseau
- ✅ **Provisioning physique** : Carte SD retirée après chargement
- ✅ **NVS chiffré** : Certificats stockés de manière sécurisée
- ✅ **Scalable** : Une carte SD pour provisionner plusieurs gateways
- ✅ **Rotation facile** : Réinsérer SD avec nouveaux certificats

## Workflow complet

```
┌─────────────────────────────────────────────────────────────┐
│  Phase 1 : Préparation carte SD (sur PC)                    │
├─────────────────────────────────────────────────────────────┤
│  1. Formater carte SD en FAT32                              │
│  2. Créer structure : /certs/                               │
│  3. Copier certificats PEM                                  │
│     - ca.crt (certificat CA)                                │
│     - client.crt (certificat client gateway)                │
│     - client.key (clé privée)                               │
│  4. Vérifier permissions (client.key = 400)                 │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│  Phase 2 : Provisioning (sur ESP32-POE-ISO)                │
├─────────────────────────────────────────────────────────────┤
│  1. ESP32 éteint → Insérer carte SD                        │
│  2. Alimenter ESP32 (PoE ou USB)                           │
│  3. Au boot : détection auto carte SD                      │
│  4. Lecture certificats depuis /sdcard/certs/              │
│  5. Stockage en NVS chiffré                                │
│  6. LED verte clignote → Succès                            │
│  7. Message série : "PROVISIONING TERMINÉ"                 │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│  Phase 3 : Retrait sécurisé                                │
├─────────────────────────────────────────────────────────────┤
│  1. Attendre message "retrait sécurisé OK"                 │
│  2. Retirer carte SD physiquement                          │
│  3. Stocker carte en lieu sûr (coffre, armoire sécurisée) │
│  4. ESP32 redémarre automatiquement                        │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│  Phase 4 : Utilisation normale (sans SD)                   │
├─────────────────────────────────────────────────────────────┤
│  1. Boot normal : chargement certificats depuis NVS        │
│  2. Connexion MQTT/TLS 1.3 automatique                     │
│  3. Publication données nœuds                               │
│  4. Carte SD absente → sécurité renforcée                  │
└─────────────────────────────────────────────────────────────┘
```

## Configuration NVS Encryption

### Option 1 : HMAC (développement) ⚡ Rapide

**Avantages** :
- Setup immédiat, aucune config supplémentaire
- Débuggage facile
- Pas de risque de "bricking" ESP32

**Limitations** :
- Clé HMAC stockée en flash non chiffré
- Extraction théoriquement possible avec dump flash

**Configuration** :
```ini
# platformio.ini
board_build.cmake_extra_args =
  -DCONFIG_NVS_ENCRYPTION=1
  -DCONFIG_NVS_SEC_KEY_PROTECTION_SCHEME=1
```

### Option 2 : Flash Encryption (production) 🔒 Sécurisé

**Avantages** :
- Flash entièrement chiffré (AES-256)
- Clé de chiffrement stockée dans eFuse (OTP)
- Lecture flash impossible sans clé

**⚠️ ATTENTION** :
- **IRRÉVERSIBLE** : Une fois activé, impossible de revenir en arrière
- Nécessite Secure Boot pour sécurité maximale
- Temps de boot légèrement augmenté (~200ms)

**Configuration** :
```ini
# platformio.ini
board_build.cmake_extra_args =
  -DCONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=1
  -DCONFIG_SECURE_BOOT_V2_ENABLED=1
  -DCONFIG_NVS_ENCRYPTION=1
  -DCONFIG_NVS_SEC_KEY_PROTECTION_SCHEME=2
```

**Activation Flash Encryption** :
```bash
# 1. Compiler firmware avec Flash Encryption
pio run

# 2. Flasher et activer (PREMIÈRE FOIS SEULEMENT)
esptool.py --port COM3 write_flash 0x1000 bootloader.bin
esptool.py --port COM3 burn_efuse FLASH_CRYPT_CNT

# ⚠️ Après cette étape, flash chiffré définitivement !
```

## Préparation carte SD

### Structure fichiers

```
📁 SDCARD (FAT32)
└── 📁 certs/
    ├── 📄 ca.crt          (1-2 KB)
    ├── 📄 client.crt      (1-2 KB)
    └── 🔐 client.key      (1-2 KB)
```

### Génération certificats

Voir [MQTT_INTEGRATION_GUIDE.md](MQTT_INTEGRATION_GUIDE.md) section "Génération certificats".

**Résumé rapide** :
```bash
# CA
openssl req -new -x509 -days 3650 -keyout ca.key -out ca.crt \
  -subj "/C=FR/O=APRU40/CN=APRU40-CA"

# Certificat client gateway
openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr \
  -subj "/C=FR/O=APRU40/CN=APRU40_GW001"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out client.crt -days 3650
```

### Copier sur carte SD

**Linux/macOS** :
```bash
# Monter SD
sudo mkdir -p /mnt/sdcard
sudo mount /dev/sdb1 /mnt/sdcard

# Copier certificats
sudo mkdir -p /mnt/sdcard/certs
sudo cp ca.crt client.crt client.key /mnt/sdcard/certs/
sudo chmod 444 /mnt/sdcard/certs/ca.crt
sudo chmod 444 /mnt/sdcard/certs/client.crt
sudo chmod 400 /mnt/sdcard/certs/client.key

# Démonter
sudo umount /mnt/sdcard
```

**Windows** :
```powershell
# Insérer carte SD (ex: lecteur E:)
mkdir E:\certs
copy ca.crt E:\certs\
copy client.crt E:\certs\
copy client.key E:\certs\

# Éjecter proprement
```

## Utilisation dans le code

### Exemple complet (gateway)

```c
// main/main.c

#if NODE_MODE == MODE_GATEWAY && ENABLE_ETHERNET

void app_main(void)
{
    // ... init Ethernet, NVS ...
    
    // Charger certificats (auto : NVS puis SD si besoin)
    char *ca_cert = NULL, *client_cert = NULL, *client_key = NULL;
    
    esp_err_t ret = mqtt_manager_load_certs(&ca_cert, 
                                             &client_cert, 
                                             &client_key);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Certificats non disponibles");
        ESP_LOGI(TAG, "Insérer carte SD et redémarrer");
        return; // Ou continuer sans MQTT
    }
    
    // Configurer MQTT avec certificats
    mqtt_manager_config_t mqtt_config = {
        .broker_uri = MQTT_BROKER_URI,
        .client_id = MQTT_CLIENT_ID,
        .ca_cert_pem = ca_cert,
        .client_cert_pem = client_cert,
        .client_key_pem = client_key,
        .msg_cb = mqtt_message_callback,
        .conn_cb = mqtt_connection_callback,
        .keepalive_sec = 60,
        .qos = 1,
        .retain = false
    };
    
    ESP_ERROR_CHECK(mqtt_manager_init(&mqtt_config));
    ESP_ERROR_CHECK(mqtt_manager_start());
    
    // Libérer mémoire certificats
    free(ca_cert);
    free(client_cert);
    free(client_key);
    
    // MQTT connecté, gateway opérationnelle
}

#endif
```

### Forcer rechargement depuis SD

```c
// Effacer NVS et forcer reprovisioning
mqtt_manager_clear_certs();
esp_restart(); // Redémarrer → charge depuis SD
```

## Logs attendus

### Premier boot (avec SD)

```
[MQTT_MGR] 🔐 Chargement certificats TLS...
[MQTT_MGR] ⚠️  Aucun certificat en NVS
[MQTT_MGR] 💡 Insérer carte SD avec /certs/ pour provisioning
[MQTT_MGR] 🔍 Tentative chargement depuis carte SD...
[MQTT_MGR] 🔍 Montage carte SD...
[MQTT_MGR] ✅ Carte SD montée: SD32G (29.72 GB)
[MQTT_MGR] 📂 Lecture certificats depuis /certs/...
[MQTT_MGR]   ✓ CA certificate
[MQTT_MGR]   ✓ Client certificate
[MQTT_MGR]   ✓ Client private key
[MQTT_MGR] 💾 Stockage en NVS chiffré...
[MQTT_MGR] ✅ Certificats stockés en NVS sécurisé
[MQTT_MGR] 
[MQTT_MGR] ╔═══════════════════════════════════════════╗
[MQTT_MGR] ║  🎉 PROVISIONING TERMINÉ AVEC SUCCÈS !   ║
[MQTT_MGR] ║                                           ║
[MQTT_MGR] ║  💡 Vous pouvez maintenant :             ║
[MQTT_MGR] ║     1. RETIRER la carte SD                ║
[MQTT_MGR] ║     2. Stocker la SD en lieu sûr          ║
[MQTT_MGR] ║     3. Redémarrer l'ESP32                 ║
[MQTT_MGR] ║                                           ║
[MQTT_MGR] ║  🔒 Les certificats restent en NVS        ║
[MQTT_MGR] ║     (chiffré si NVS Encryption activé)    ║
[MQTT_MGR] ╚═══════════════════════════════════════════╝
[MQTT_MGR] 
[MQTT_MGR] 📤 Carte SD démontée (retrait sécurisé OK)
[MQTT_MGR] ✅ Certificats chargés depuis NVS
[MQTT_MGR] ✅ Certificats OK (provisioning SD terminé)
```

### Boots suivants (sans SD)

```
[MQTT_MGR] 🔐 Chargement certificats TLS...
[MQTT_MGR] ✅ Certificats chargés depuis NVS
[MQTT_MGR]    CA: 1234 octets
[MQTT_MGR]    Cert: 1456 octets
[MQTT_MGR]    Key: 1678 octets
[MQTT_MGR] ✅ Certificats OK (depuis NVS)
[MQTT_MGR] MQTT manager initialisé (broker: mqtts://...)
[MQTT_MGR] Client MQTT démarré
[MQTT_MGR] ✅ Connecté au broker MQTT
```

## Dépannage

### Erreur "Échec montage carte SD"

**Causes** :
- Carte non insérée ou mal insérée
- Format non FAT32
- Carte corrompue
- Problème GPIO/pins

**Solutions** :
```bash
# Vérifier format
sudo fdisk -l /dev/sdb
# Doit afficher : FAT32 ou W95 FAT32

# Reformater si besoin
sudo mkfs.vfat -F 32 /dev/sdb1

# Vérifier intégrité
sudo fsck.vfat -a /dev/sdb1
```

### Erreur "Échec lecture certificat"

**Causes** :
- Dossier `/certs/` absent
- Noms fichiers incorrects
- Permissions lecture refusées
- Certificats corrompus

**Vérifications** :
```bash
# Structure exacte requise
ls -lh /mnt/sdcard/certs/
# Doit afficher :
# ca.crt
# client.crt
# client.key

# Permissions OK ?
chmod 444 /mnt/sdcard/certs/ca.crt
chmod 444 /mnt/sdcard/certs/client.crt
chmod 400 /mnt/sdcard/certs/client.key

# Vérifier format PEM
openssl x509 -in /mnt/sdcard/certs/ca.crt -text -noout
# Doit afficher infos certificat
```

### Certificats chargés mais MQTT échoue

**Vérifier certificat CA** :
```bash
# CA doit matcher le serveur Mosquitto
openssl verify -CAfile ca.crt client.crt
# Doit afficher : client.crt: OK
```

**Vérifier CN certificat client** :
```bash
openssl x509 -in client.crt -noout -subject
# CN doit correspondre au client_id MQTT
```

### Rotation certificats

```c
// 1. Préparer nouvelle SD avec nouveaux certificats
// 2. Insérer SD dans gateway allumée
// 3. Forcer rechargement :
mqtt_manager_clear_certs();
mqtt_manager_load_certs_from_sd("/sdcard");
// 4. Redémarrer
esp_restart();
```

## Sécurité production

### Checklist

- [ ] Flash Encryption activé (`CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE`)
- [ ] Secure Boot activé (`CONFIG_SECURE_BOOT_V2_ENABLED`)
- [ ] NVS Encryption activé (`CONFIG_NVS_ENCRYPTION`)
- [ ] Certificats client avec date expiration (3650 jours max)
- [ ] Carte SD stockée en lieu sûr après provisioning
- [ ] Logs production filtrés (pas d'affichage certificats)
- [ ] Accès physique ESP32 restreint

### Rotation périodique

Recommandation : **Rotation tous les 1-2 ans**

Processus :
1. Générer nouveaux certificats avec OpenSSL
2. Signer avec CA existante
3. Préparer carte SD
4. Provisionner toutes les gateways (tour de site)
5. Vérifier connexions MQTT OK
6. Stocker anciens certificats (backup 90 jours)

---

**Prochaines étapes** :
- Tester provisioning sur hardware réel
- Implémenter rotation automatique via MQTT
- Ajouter monitoring expiration certificats
- Créer script automation provisioning multi-gateways
