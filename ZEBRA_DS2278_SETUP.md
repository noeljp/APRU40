# Configuration Scanner Zebra DS2278 - Procédure Complète

## 🎯 Objectif

Connecter le scanner de codes-barres **Zebra DS2278** en Bluetooth Classic (SPP) à l'ESP32-POE-ISO de manière **sécurisée** et **audit-friendly**.

---

## ⚠️ Important : Ordre des étapes

```
1. Sécurité     (Medium Bluetooth Security)
2. Profil       (SPP Non-Discoverable Central Mode)
3. Pairing      (Code-barres spécial + PIN)
4. Verrouillage (Non-discoverable permanent)
```

**Ne pas inverser l'ordre** : cela garantit une configuration sécurisée dès le départ.

---

## 📋 Étape 0 : Reset usine (recommandé)

**Scanner ce code-barres** : `Set Defaults`

![Set Defaults](https://www.zebra.com/content/dam/zebra_new_ia/en-us/manuals/scanners/ds2200-product-reference-guide-en.pdf)

**Objectif** : Partir d'un état propre, sans configurations résiduelles.

**Indicateur** : Le scanner émet 3 bips et redémarre.

---

## 🔐 Étape 1 : Forcer la sécurité (ANTI "Just Works")

### ❌ Problème : "Just Works" = connexion sans authentification

Par défaut, le DS2278 peut accepter des connexions **non sécurisées** :
- Pas de PIN
- Vulnérable aux attaques MITM
- Non conforme aux audits de sécurité

### ✅ Solution : Medium Bluetooth Security

**Scanner ce code-barres** : `Medium Bluetooth Security`

**Effet** :
- 🔐 **Passkey obligatoire** (6 chiffres minimum)
- 🔐 **Protection MITM** (Man-In-The-Middle)
- 🔐 **Lien chiffré** (AES-128)
- ✅ **Conforme audits**

**Vérification** : Le scanner affiche "Medium Security" en mode config.

---

## 📡 Étape 2 : Configurer le profil Bluetooth

### Mode requis : SPP (Serial Port Profile)

**Scanner ce code-barres** : `Bluetooth Classic – SPP (Non-Discoverable / Central Mode)`

**Effet** :
- ✅ **SPP uniquement** (pas de HID, pas d'autres profils)
- ✅ **Non-Discoverable** : invisible aux scans Bluetooth externes
- ✅ **Central Mode** : le scanner se connecte à l'ESP32 (et non l'inverse)

**Pourquoi Central Mode ?**
- L'ESP32 agit en **serveur SPP** (fixe, toujours disponible)
- Le scanner agit en **client SPP** (mobile, se connecte au besoin)

---

## 🖥️ Étape 3 : Préparer l'ESP32 (configuration logicielle)

### Configuration Bluetooth sur ESP32

```c
// Dans main.c - Configuration du scanner
#define BT_SCANNER_NAME       "TAS-MACHINE-QR-01"   // Nom unique par machine
#define BT_SCANNER_PIN        "736281"              // PIN fixe 6+ chiffres
#define BT_SCANNER_WHITELIST  true                  // Activer whitelist après pairing
```

### Adresse Bluetooth de l'ESP32

Au démarrage, l'ESP32 affiche son adresse BT dans les logs :

```
I (1234) BT_SPP: Adresse BT: 7C:DF:A1:92:3B:10
```

**👉 Noter cette adresse** : vous en aurez besoin pour le code-barres de pairing.

### Mode discoverable temporaire

Pour le pairing initial uniquement :

```c
bt_spp_config_t config = {
    .device_name = BT_SCANNER_NAME,
    .pin_code = BT_SCANNER_PIN,
    .discoverable_at_init = true,   // ✅ OUI au démarrage (pour pairing)
    .whitelist_addr = NULL,         // ❌ Pas de whitelist AVANT pairing
    // ...
};
```

**Après pairing** : Redéployer avec `discoverable_at_init = false`.

---

## 🔗 Étape 4 : Créer le code-barres de pairing

### Format spécial Zebra (Code 128)

Le DS2278 utilise un **code-barres spécial** pour le pairing Bluetooth :

```
<FNC3>B7CDFA1923B10
```

**Décomposition** :
- `<FNC3>` : Caractère spécial Zebra (obligatoire)
- `B` : Bluetooth SPP
- `7CDFA1923B10` : Adresse MAC ESP32 **sans les `:`**

### Exemple concret

Si votre ESP32 a l'adresse : `7C:DF:A1:92:3B:10`

Contenu du code-barres :
```
<FNC3>B7CDFA1923B10
```

### Génération du code-barres

**Option A : Générateur en ligne** (recommandé)
1. Aller sur : https://barcode.tec-it.com/en/Code128
2. Type : `Code 128`
3. Text : `<FNC3>B7CDFA1923B10` (remplacer par votre MAC)
4. Télécharger l'image PNG

**Option B : Zebra Designer**
- Créer un code-barres Code 128
- Insérer le FNC3 via le menu spécial

**Option C : Affichage écran**
- Générer le code et afficher sur un écran
- Scanner directement depuis l'écran

**👉 Imprimer ou afficher ce code-barres** : il servira UNE SEULE FOIS.

---

## 🔑 Étape 5 : Pairing (appairage unique)

### Procédure

1. **ESP32 allumé** et mode `discoverable = true`
2. **Scanner le code-barres de pairing** créé à l'étape 4
3. Le DS2278 affiche : `Enter PIN`
4. **Entrer le PIN** configuré sur l'ESP32 (ex: `736281`)
5. **Bip de confirmation** 🔔

### Vérification

Logs ESP32 :

```
I (5678) BT_SPP: Client SPP connecté: A0:B1:C2:D3:E4:F5
I (5679) BT_SPP: Authentification réussie
```

Le scanner est maintenant **appairé définitivement** avec cet ESP32.

### En cas d'échec

- **Erreur PIN** : Vérifier que le PIN ESP32 correspond
- **Pas de connexion** : Vérifier que l'ESP32 est en mode `discoverable`
- **Whitelist active** : Désactiver temporairement pour le pairing initial

---

## 🔒 Étape 6 : Verrouillage post-provisioning (sécurité finale)

### Après appairage réussi

#### Sur l'ESP32

1. **Récupérer l'adresse BT du scanner** (dans les logs) :
   ```
   I (5678) BT_SPP: Client SPP connecté: A0:B1:C2:D3:E4:F5
   ```

2. **Activer la whitelist** dans le code :
   ```c
   // Adresse BT du scanner Zebra (récupérée lors du premier pairing)
   static const uint8_t scanner_bt_addr[6] = {0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5};
   
   bt_spp_config_t config = {
       .device_name = BT_SCANNER_NAME,
       .pin_code = BT_SCANNER_PIN,
       .discoverable_at_init = false,    // ❌ NON après pairing
       .whitelist_addr = scanner_bt_addr, // ✅ OUI, uniquement ce scanner
       // ...
   };
   ```

3. **Redéployer le firmware** avec ces paramètres

#### Résultat final

- ✅ ESP32 **non-discoverable** : invisible aux scans externes
- ✅ Whitelist active : seul le scanner appairé peut se connecter
- ✅ Reconnexion automatique : le scanner se reconnecte à l'allumage
- ✅ **Aucun nouvel appairage possible** sans reset

**État audit-clean** : Configuration verrouillée et prévisible en production.

---

## 📨 Format des données

### Configuration scanner

Le DS2278 doit être configuré pour envoyer :

```
<DATA><CR><LF>
```

**Scanner ce code-barres** : `Suffix CR+LF`

### Exemple de données reçues

```
TAS|MACHINE|TVAC09|TRAY|A17\r\n
EAN13|3760123456789\r\n
QR|https://example.com/asset/12345\r\n
```

### Format recommandé

**Structure** : `TYPE|FIELD1|FIELD2|...|FIELDn`

**Exemples** :
- Identification plateau : `TAS|MACHINE|TVAC09|TRAY|A17`
- Code EAN : `EAN13|3760123456789`
- QR Code : `QR|https://example.com/asset/12345`

---

## 🛡️ Validation côté ESP32 (callback)

### Code de traitement

```c
void scanner_data_callback(const char *line, size_t len) {
    ESP_LOGI(TAG, "Scanner RX: %s (len=%d)", line, len);
    
    // Validation longueur
    if (len < 5 || len > 200) {
        ESP_LOGW(TAG, "Ligne invalide (longueur)");
        return;
    }
    
    // Validation préfixe
    if (strncmp(line, "TAS|", 4) == 0) {
        // Traiter données TAS
        parse_tas_data(line);
    } else if (strncmp(line, "EAN13|", 6) == 0) {
        // Traiter code EAN
        parse_ean13(line + 6);
    } else if (strncmp(line, "QR|", 3) == 0) {
        // Traiter QR code
        parse_qr_code(line + 3);
    } else {
        ESP_LOGW(TAG, "Format non reconnu: %s", line);
    }
}
```

### Règles de sécurité

✅ **À faire** :
- Vérifier longueur min/max
- Valider préfixe attendu
- Parser avec strtok_r (thread-safe)
- Rejeter si non conforme
- Timestamp + log

❌ **Pas besoin** :
- Crypto applicative lourde (Bluetooth déjà chiffré)
- Signature HMAC (surface d'attaque minuscule)

---

## 🔧 Dépannage

### Problème : Scanner ne se connecte pas

**Causes possibles** :
1. ESP32 non-discoverable lors du pairing initial
2. Code-barres de pairing incorrect (MAC mal formatée)
3. PIN incorrect
4. Whitelist activée avant premier pairing

**Solutions** :
1. Vérifier `discoverable_at_init = true` au premier démarrage
2. Régénérer le code-barres avec la bonne adresse MAC
3. Vérifier que le PIN ESP32 correspond
4. Désactiver whitelist pour le pairing initial

### Problème : Connexion OK mais pas de données

**Causes possibles** :
1. Scanner configuré en mode HID (pas SPP)
2. Format de données incorrect (pas de CR+LF)

**Solutions** :
1. Scanner le code `SPP Mode`
2. Scanner le code `Suffix CR+LF`

### Problème : Déconnexions fréquentes

**Causes possibles** :
1. Interférences Bluetooth (ESP-NOW actif sur 2.4 GHz)
2. Distance trop grande (>10m)
3. Obstacles métalliques

**Solutions** :
1. Vérifier compatibilité Bluetooth Classic + ESP-NOW simultané
2. Rapprocher scanner et ESP32
3. Repositionner les appareils

---

## ⚠️ Notes importantes

### Compatibilité Bluetooth Classic + ESP-NOW

L'ESP32 supporte :
- ✅ **Bluetooth Classic + WiFi** simultanément (OK)
- ⚠️ **BLE + ESP-NOW** : possibles interférences (même bande 2.4 GHz)

**Bluetooth Classic (SPP)** utilise une bande de fréquence légèrement différente de WiFi 2.4 GHz, donc **cohabitation possible** avec ESP-NOW.

### Consommation

- **Bluetooth Classic actif** : ~30 mA supplémentaires
- **Total avec ESP-NOW** : ~90 mA (acceptable avec POE)

### Portée effective

- **Bluetooth Classic** : 10-15m en intérieur
- **Scanner Zebra DS2278** : Classe 2 (10m max)

---

## 📚 Codes-barres de configuration Zebra

Pour faciliter la configuration, voici les codes-barres principaux :

| Configuration | Code à scanner |
|---------------|----------------|
| Reset usine | `Set Defaults` |
| Medium Security | `Medium Bluetooth Security` |
| SPP Non-Discoverable | `Bluetooth Classic – SPP (Non-Discoverable / Central Mode)` |
| Suffix CR+LF | `Suffix CR+LF` |

**Source** : [Zebra DS2278 Product Reference Guide](https://www.zebra.com/content/dam/zebra_new_ia/en-us/manuals/scanners/ds2200-product-reference-guide-en.pdf)

---

## 🚀 Résumé de la procédure

```
1. ✅ Reset usine
2. ✅ Scanner "Medium Bluetooth Security"
3. ✅ Scanner "SPP Non-Discoverable Central Mode"
4. ✅ Noter adresse BT ESP32 (ex: 7C:DF:A1:92:3B:10)
5. ✅ Générer code-barres pairing : <FNC3>B7CDFA1923B10
6. ✅ ESP32 en mode discoverable (temporaire)
7. ✅ Scanner le code-barres de pairing
8. ✅ Entrer PIN (ex: 736281)
9. ✅ Bip de confirmation
10. ✅ Noter adresse BT scanner (ex: A0:B1:C2:D3:E4:F5)
11. ✅ Redéployer ESP32 avec whitelist + non-discoverable
12. ✅ Scanner "Suffix CR+LF"
13. ✅ Test final : scanner un code et vérifier réception

→ Configuration verrouillée et sécurisée ! 🔒
```

---

## 🎓 Pour aller plus loin

### Génération automatique du code-barres de pairing

Script Python pour générer automatiquement le code-barres :

```python
import barcode
from barcode.writer import ImageWriter

esp32_mac = "7C:DF:A1:92:3B:10"  # Remplacer par votre MAC
mac_clean = esp32_mac.replace(":", "")

# Contenu du code-barres
content = f"\xf3B{mac_clean}"  # \xf3 = FNC3 en Code 128

# Générer
code128 = barcode.get('code128', content, writer=ImageWriter())
code128.save('zebra_pairing_barcode')

print(f"Code-barres généré : zebra_pairing_barcode.png")
```

### Déploiement en production

Pour un déploiement sur 30 machines :

1. **Flasher tous les ESP32** avec le même firmware (discoverable = true)
2. **Pairing** : Scanner le code-barres unique par machine
3. **Récupérer toutes les adresses BT des scanners**
4. **Générer 30 firmwares** avec whitelist spécifique par machine
5. **Reflasher** chaque ESP32 avec son firmware (non-discoverable + whitelist)

**Automatisation possible** avec PlatformIO build_flags :

```ini
[env:machine01]
build_flags = 
    -DBT_SCANNER_ADDR=0xA0,0xB1,0xC2,0xD3,0xE4,0xF5
    -DBT_DISCOVERABLE=false
```

---

**Configuration terminée !** Le scanner Zebra DS2278 est maintenant connecté de manière sécurisée à l'ESP32.
