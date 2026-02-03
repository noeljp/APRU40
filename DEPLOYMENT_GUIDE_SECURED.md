# Guide de Déploiement Sécurisé - APRU40

**Version** : 2.0  
**Date** : 1 février 2026  
**Conformité** : NIS2 Article 21 - Gestion des risques de cybersécurité

---

## 📋 Table des Matières

1. [Préparation Matériel](#1-préparation-matériel)
2. [Configuration Initiale](#2-configuration-initiale)
3. [Déploiement sur Site](#3-déploiement-sur-site)
4. [Vérification Sécurité](#4-vérification-sécurité)
5. [Procédure Tamper](#5-procédure-tamper)
6. [Troubleshooting](#6-troubleshooting)

---

## 1. Préparation Matériel

### 1.1 Checklist par Nœud

- [ ] ESP32-WROOM-32 ou ESP32-POE-ISO
- [ ] Boîtier inviolable avec switch tamper
- [ ] Scanner Zebra DS2278 (Bluetooth)
- [ ] Capteurs (ADS7128, ADS1119, TCA9537)
- [ ] Alimentation PoE ou 5V USB
- [ ] Carte SD (provisioning certificats pour gateways)
- [ ] Scellés de sécurité numérotés

### 1.2 Outils Requis

- [ ] Câble USB (flashage)
- [ ] Ordinateur avec PlatformIO
- [ ] Smartphone/Tablet (noter PIN Bluetooth)
- [ ] Tournevis T10 (ouverture boîtier)
- [ ] Multimètre (test tamper switch)

---

## 2. Configuration Initiale

### 2.1 Compiler le Firmware

```bash
cd APRU40
platformio run --environment esp32-poe-iso
```

### 2.2 Configurer node_config.h

**IMPORTANT** : Chaque nœud doit avoir une configuration unique.

```c
// node_config.h - Nœud 01

#define NODE_ID         1
#define NODE_NAME       "APRU40-Node-01"

// GPIO tamper (GPIO34 recommandé - input-only)
#define TAMPER_GPIO                 GPIO_NUM_34
#define TAMPER_ACTIVE_LOW           true
#define TAMPER_AUTO_ERASE_NVS       true    // ⚠️ PRODUCTION
#define TAMPER_AUTO_RESTART         true

// MAC scanner Zebra DS2278 (relever depuis scanner)
// Settings > About > Bluetooth Address
#define BT_SCANNER_MAC_WHITELIST    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}
```

**⚠️ CRITIQUE** : 
- `TAMPER_AUTO_ERASE_NVS = true` en PRODUCTION → Efface clés si ouverture
- `TAMPER_AUTO_ERASE_NVS = false` en DEBUG → Logs uniquement (tests)

### 2.3 Flasher l'ESP32

```bash
# Connecter ESP32 via USB
platformio run --target upload --environment esp32-poe-iso

# Monitorer les logs (noter le PIN Bluetooth)
platformio device monitor
```

**Exemple de logs :**

```
I (1234) BT_SEC: ═══════════════════════════════════════════════════
I (1235) BT_SEC: 🔐 BLUETOOTH SECURITY - Configuration
I (1236) BT_SEC: ═══════════════════════════════════════════════════
I (1237) BT_SEC: 📱 Device:      APRU40-Node-01
I (1238) BT_SEC: 🔑 PIN:         736428
I (1239) BT_SEC: 📡 Scanner MAC: 12:34:56:78:9A:BC
I (1240) BT_SEC: 🔒 Whitelist:   STRICTE (1 scanner autorisé)
I (1241) BT_SEC: ═══════════════════════════════════════════════════
```

**✅ NOTER LE PIN : 736428** (unique par nœud, persistant en NVS)

### 2.4 Test Tamper en Laboratoire

**⚠️ OBLIGATOIRE AVANT DÉPLOIEMENT**

1. Connecter un switch temporaire sur GPIO34
2. Monitorer les logs
3. Activer le switch → Vérifier logs :

```
E (5678) TAMPER_SEC: ═══════════════════════════════════════════════════
E (5679) TAMPER_SEC: 🚨 ALERTE SÉCURITÉ : TAMPER DÉTECTÉ (#1)
E (5680) TAMPER_SEC: ═══════════════════════════════════════════════════
W (5681) TAMPER_SEC: ⚠️ Envoi alerte tamper...
W (5682) TAMPER_SEC: 🗑️ EFFACEMENT NVS...
I (5683) TAMPER_SEC: ✅ NVS effacé avec succès
E (5684) TAMPER_SEC: 🔴 DISPOSITIF COMPROMIS - REDÉMARRAGE IMMINENT
```

4. Vérifier redémarrage automatique
5. Vérifier que le PIN Bluetooth est régénéré (nouveau PIN après reboot)

**✅ Test réussi** → Procéder au déploiement

---

## 3. Déploiement sur Site

### 3.1 Installation Physique

1. **Fixer le nœud** sur support (mur, machine, rack)
2. **Connecter capteurs** (I2C, GPIO)
3. **Connecter switch tamper** (GPIO34 → Boîtier)
4. **Tester continuité tamper** avec multimètre (circuit fermé normalement)
5. **Alimenter** (PoE ou 5V)

### 3.2 Pairing Scanner Zebra DS2278

**⚠️ FENÊTRE DE 5 MINUTES APRÈS ACTIVATION DISCOVERABLE**

1. **Activer mode discoverable** (commande à envoyer via console ou bouton physique) :

```c
// Dans le code ou via commande série
bt_security_set_discoverable(true, 300);  // 300 sec = 5 min
```

2. **Logs ESP32** :

```
I (1234) BT_SEC: 🕐 Discoverable activé pour 300 secondes
```

3. **Scanner Zebra DS2278** :
   - Scanner le code-barres **"Bluetooth Classic – SPP (Non-Discoverable / Central Mode)"** (voir [ZEBRA_DS2278_SETUP.md](ZEBRA_DS2278_SETUP.md))
   - Scanner cherche devices Bluetooth
   - Sélectionner **"APRU40-Node-01"**
   - Entrer PIN : **736428** (exemple, le vôtre est dans les logs)

4. **Logs ESP32 connexion** :

```
I (5678) BT_SEC: ✅ Connexion autorisée (scanner Zebra)
I (5679) BT_SEC:    Total connexions: 1
```

5. **Test scan** :
   - Scanner un code-barres avec le Zebra
   - Vérifier réception dans logs :

```
I (6789) APRU40_SEC: 📱 Scanner: TAS|123456
I (6790) APRU40_SEC: Type: TAS, Code: 123456
```

**✅ Pairing réussi** → Scanner associé de façon permanente

### 3.3 Fermeture Sécurisée

1. **Fermer le boîtier**
2. **Vérifier logs tamper** :

```
I (7890) TAMPER_SEC: ✅ État tamper OK (boîtier fermé)
```

3. **Apposer scellé numéroté** (ex: #001234)
4. **Enregistrer dans CMDB** :
   - Node ID: 1
   - Nom: APRU40-Node-01
   - PIN Bluetooth: 736428
   - MAC Scanner: 12:34:56:78:9A:BC
   - Scellé: #001234
   - Date déploiement: 2026-02-01
   - Opérateur: Jean Dupont

---

## 4. Vérification Sécurité

### 4.1 Checklist Post-Déploiement

- [ ] **Tamper** : Switch détecté, état OK dans logs
- [ ] **Bluetooth** : Scanner pairé, connexion réussie
- [ ] **Whitelist** : Tentative connexion autre device = rejetée
- [ ] **Scellé** : Numéroté, enregistré dans CMDB
- [ ] **ESP-NOW** : Données reçues par gateway
- [ ] **Heartbeat** : Logs toutes les 10 secondes

### 4.2 Test Sécurité Bluetooth

**⚠️ TEST CRITIQUE : Vérifier rejet device non autorisé**

1. Avec un smartphone, chercher devices Bluetooth
2. **APRU40-Node-01 NE DOIT PAS APPARAÎTRE** (discoverable désactivé)
3. Si visible → PROBLÈME DE CONFIGURATION

**Si un device non autorisé tente de se connecter** :

```
E (8901) BT_SEC: 🚫 CONNEXION REFUSÉE : MAC non autorisé
E (8902) BT_SEC:    Device: AA:BB:CC:DD:EE:FF
E (8903) BT_SEC:    Attendu: 12:34:56:78:9A:BC
W (8904) APRU40_SEC: Alerte BT unauthorized envoyée (ESP-NOW)
```

**✅ Rejet confirmé** → Sécurité Bluetooth OK

---

## 5. Procédure Tamper

### 5.1 Intervention Autorisée (Maintenance)

**⚠️ PROCÉDURE POUR ÉVITER EFFACEMENT NVS**

1. **Désactiver tamper temporairement** (avant ouverture) :

```c
// Via commande série ou bouton physique
tamper_security_disable();
```

2. **Logs** :

```
W (9012) TAMPER_SEC: 🔓 Tamper DÉSACTIVÉ (maintenance)
```

3. **Ouvrir boîtier** (pas d'alerte)
4. **Effectuer maintenance**
5. **Fermer boîtier**
6. **Réactiver tamper** :

```c
tamper_security_enable();
```

7. **Logs** :

```
I (9345) TAMPER_SEC: 🔒 Tamper RÉACTIVÉ
```

**✅ Maintenance terminée sans effacement**

### 5.2 Détection Intrusion

**Si tamper déclenché sans désactivation préalable :**

1. **Alerte ESP-NOW/MQTT envoyée** vers gateway
2. **NVS effacé** (clés, certificats, configuration sensible)
3. **Redémarrage automatique**
4. **Nouveau PIN généré** (l'ancien est perdu)

**Logs intrusion** :

```
E (5678) TAMPER_SEC: 🚨 ALERTE SÉCURITÉ : TAMPER DÉTECTÉ (#1)
W (5679) TAMPER_SEC: ⚠️ Envoi alerte tamper...
W (5680) APRU40_SEC: Alerte tamper envoyée (ESP-NOW)
W (5681) TAMPER_SEC: 🗑️ EFFACEMENT NVS...
I (5682) TAMPER_SEC: ✅ NVS effacé avec succès
E (5683) TAMPER_SEC: 🔴 DISPOSITIF COMPROMIS - REDÉMARRAGE IMMINENT
```

**Après redémarrage :**

```
W (1234) TAMPER_SEC: ⚠️ TAMPER DÉTECTÉ AU BOOT ! Boîtier ouvert.
```

**Actions requises** :

1. **Reflasher firmware** (clés ESP-NOW à recharger)
2. **Re-pairing scanner** (nouveau PIN généré)
3. **Audit forensique** (qui, quand, pourquoi ?)
4. **Rapport incident NIS2** (si infrastructure critique)

---

## 6. Troubleshooting

### 6.1 Tamper ne se déclenche pas

**Symptômes** : Ouverture boîtier sans alerte

**Diagnostic** :

```bash
# Vérifier GPIO tamper
I (1234) TAMPER_SEC: Tamper security initialisé
I (1235) TAMPER_SEC:   GPIO:          34
I (1236) TAMPER_SEC:   Active Low:    OUI
I (1237) TAMPER_SEC:   Auto-erase:    OUI
```

**Solutions** :

1. Vérifier câblage switch → GPIO34
2. Tester continuité avec multimètre
3. Vérifier `TAMPER_ACTIVE_LOW` dans config
4. Tester avec `tamper_security_test()` :

```c
tamper_security_test();  // Simule déclenchement
```

### 6.2 Scanner Bluetooth ne se connecte pas

**Symptômes** : Scanner ne trouve pas le nœud

**Diagnostic** :

1. Vérifier discoverable actif :

```c
bt_security_set_discoverable(true, 300);
```

2. Logs :

```
I (1234) BT_SEC: 🕐 Discoverable activé pour 300 secondes
```

3. Scanner cherche → **APRU40-Node-01** doit apparaître

**Solutions** :

- Attendre fin initialisation Bluetooth (30 secondes)
- Redémarrer scanner Zebra
- Vérifier distance (<10 mètres)
- Vérifier scanner en mode **"Bluetooth Classic SPP"** (pas BLE)

### 6.3 Connexion Bluetooth refusée

**Symptômes** : Scanner se connecte puis déconnexion immédiate

**Logs** :

```
E (5678) BT_SEC: 🚫 CONNEXION REFUSÉE : MAC non autorisé
E (5679) BT_SEC:    Device: AA:BB:CC:DD:EE:FF
E (5680) BT_SEC:    Attendu: 12:34:56:78:9A:BC
```

**Solutions** :

1. **Vérifier MAC scanner dans config** :
   - Scanner Zebra : Settings > About > Bluetooth Address
   - Comparer avec `BT_SCANNER_MAC_WHITELIST` dans node_config.h
2. **Mettre à jour config** :

```c
#define BT_SCANNER_MAC_WHITELIST    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
```

3. **Recompiler + reflasher**

### 6.4 PIN Bluetooth perdu

**Symptômes** : Reboot du nœud, nouveau PIN généré

**Récupération** :

1. Le PIN est stocké en NVS (persistant entre reboots)
2. Si NVS effacé (tamper, erase) → Nouveau PIN généré
3. Consulter logs au boot :

```
I (1238) BT_SEC: 🔑 PIN:         523914
```

4. **Re-pairing nécessaire** si PIN changé

**Prévention** :

- Noter PIN dans CMDB après déploiement
- Éviter `nvs_flash_erase()` en production
- Backup NVS si possible (pas supporté par défaut ESP32)

---

## 7. Conformité NIS2

### 7.1 Exigences Couvertes

| Article NIS2 | Exigence | Implémentation |
|--------------|----------|----------------|
| Art. 21.1.a | Gestion des risques | Tamper + Whitelist BT |
| Art. 21.1.b | Gestion incidents | Alertes ESP-NOW/MQTT |
| Art. 21.1.e | Sécurité physique | Switch tamper + scellés |
| Art. 21.1.f | Contrôle d'accès | Whitelist MAC stricte |
| Art. 21.2.a | Chiffrement | ESP-NOW AES-256 + HMAC |
| Art. 21.2.c | Authentification forte | PIN aléatoire + mTLS (gateway) |

### 7.2 Registre Incidents

**À maintenir en cas de déclenchement tamper :**

| Date | Nœud | Type | Détails | Actions |
|------|------|------|---------|---------|
| 2026-02-01 14:32 | Node-01 | Tamper | Boîtier ouvert, NVS effacé | Reflash + audit |
| 2026-02-15 09:15 | Node-03 | BT Unauthorized | MAC AA:BB:CC:DD:EE:FF refusé | Investigation |

---

## 8. Contacts Support

| Type | Contact | Disponibilité |
|------|---------|---------------|
| **Support technique** | support@apru40.com | 24/7 |
| **Sécurité (RSSI)** | rssi@apru40.com | Business hours |
| **Incidents critiques** | incidents@apru40.com | 24/7 |
| **CERT-FR (NIS2)** | cert-fr.cossi@ssi.gouv.fr | Incidents > 24h |

---

**FIN DU GUIDE**

**Révision** : 1.0  
**Prochaine mise à jour** : 1er août 2026
