# Cahier des Charges - Interface d'Administration APRU40

**Projet** : Plateforme de Gestion Réseau IoT APRU40  
**Date** : 2 février 2026  
**Version** : 1.0  
**Client** : Interne  
**Conformité** : NIS2 (EU) 2022/2555

---

## 📋 Table des Matières

1. [Contexte et Objectifs](#1-contexte-et-objectifs)
2. [Architecture Système](#2-architecture-système)
3. [Spécifications Fonctionnelles](#3-spécifications-fonctionnelles)
4. [Spécifications Techniques](#4-spécifications-techniques)
5. [Sécurité et Conformité](#5-sécurité-et-conformité)
6. [Interfaces Utilisateur](#6-interfaces-utilisateur)
7. [API et Intégrations](#7-api-et-intégrations)
8. [Stack Technologique](#8-stack-technologique)
9. [Planning et Phases](#9-planning-et-phases)
10. [Budget et Ressources](#10-budget-et-ressources)

---

## 1. Contexte et Objectifs

### 1.1 Contexte

Le système APRU40 est un réseau IoT industriel composé de :
- **30+ nœuds capteurs** par gateway (ESP32)
- **1-5 gateways** par site (ESP32-POE-ISO)
- **1 broker MQTT centralisé** (Mosquitto)
- **Communication sécurisée** : ESP-NOW (AES-256 + HMAC) + MQTT/TLS 1.3

**Problématique actuelle** :
- Aucune interface de gestion centralisée
- Configuration manuelle des nœuds (node_config.h + reflash)
- Pas de visibilité temps réel sur l'état du réseau
- Gestion manuelle des incidents de sécurité (tamper, BT unauthorized)
- Pas d'inventaire dynamique des dispositifs

### 1.2 Objectifs

**Objectif principal** : Créer une plateforme web de gestion centralisée du réseau IoT APRU40 pour :

| Objectif | Description | Priorité |
|----------|-------------|----------|
| **Visibilité** | Vue temps réel de tous les dispositifs (30+ nœuds × 5 gateways = 150+ devices) | 🔴 P0 |
| **Configuration** | Provisioning et mise à jour configuration à distance (OTA) | 🔴 P0 |
| **Monitoring** | Surveillance santé réseau (heartbeat, connectivité, capteurs) | 🔴 P0 |
| **Sécurité** | Gestion alertes tamper, BT unauthorized, révocation nœuds compromis | 🔴 P0 |
| **Conformité NIS2** | Audit trail, rapports incidents, gestion certificats TLS | 🟠 P1 |
| **Analytique** | Historique données capteurs, tendances, anomalies | 🟡 P2 |

### 1.3 Périmètre

**Inclus dans le projet** :
- ✅ Interface web responsive (desktop + tablet)
- ✅ Backend API REST + WebSocket (temps réel)
- ✅ Base de données (inventaire, historique, logs)
- ✅ Authentification multi-utilisateurs (RBAC)
- ✅ Tableaux de bord temps réel
- ✅ Gestion configuration dispositifs
- ✅ Système d'alertes et notifications
- ✅ Rapports et exports (CSV, PDF)
- ✅ Documentation API

**Hors périmètre** :
- ❌ Modification firmware ESP32 (hors interface)
- ❌ Infrastructure serveur MQTT (existante)
- ❌ Applications mobiles natives (iOS/Android)
- ❌ Intégration ERP/SAP (phase 2)

---

## 2. Architecture Système

### 2.1 Vue d'Ensemble

```
┌─────────────────────────────────────────────────────────────────┐
│  NAVIGATEUR WEB (Frontend)                                      │
│  - React.js / Vue.js / Angular                                  │
│  - Tableaux de bord temps réel (WebSocket)                      │
│  - Gestion configuration dispositifs                            │
│  - Alertes et notifications                                     │
└───────────────────────────┬─────────────────────────────────────┘
                            │ HTTPS (REST + WebSocket)
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  BACKEND API (Node.js / Python / Go)                            │
│  - API REST (authentification, CRUD)                            │
│  - WebSocket Server (temps réel)                                │
│  - Business Logic (gestion alertes, OTA, révocation)           │
│  - Intégration MQTT (abonnement topics alertes)                │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                ┌───────────┼───────────┐
                ▼           ▼           ▼
        ┌──────────┐  ┌─────────┐  ┌──────────────┐
        │ DATABASE │  │  MQTT   │  │ FILE STORAGE │
        │ MongoDB  │  │ Broker  │  │ Certificats  │
        │ PostgreSQL│  │Mosquitto│  │ Logs         │
        └──────────┘  └─────────┘  └──────────────┘
                            │
                            │ MQTT/TLS 1.3
                            ▼
        ┌───────────────────────────────────────┐
        │  GATEWAYS ESP32-POE-ISO (1-5)         │
        │  - Topics: apru40/+/data              │
        │  - Topics: apru40/+/alert/#           │
        │  - Topics: apru40/+/status            │
        └───────────────────────────────────────┘
                            │ ESP-NOW
                            ▼
        ┌───────────────────────────────────────┐
        │  NŒUDS ESP32 (30+ par gateway)        │
        │  - Capteurs (ADS7128, ADS1119)        │
        │  - Scanner Zebra DS2278 (Bluetooth)   │
        │  - Tamper security                     │
        └───────────────────────────────────────┘
```

### 2.2 Flux de Données

**Données remontées (Nœuds → Backend)** :

1. **Données capteurs** : 
   - `apru40/gateway-01/node/01/data` → JSON capteurs (toutes les 10s)
   - Stockage en base + diffusion WebSocket vers frontend

2. **Heartbeat** :
   - `apru40/gateway-01/node/01/status` → Timestamp (toutes les 30s)
   - Détection nœuds offline (seuil 60s)

3. **Alertes sécurité** :
   - `apru40/alert/tamper/node-01` → Tamper détecté
   - `apru40/alert/bt_unauthorized/node-01` → Tentative BT non autorisée
   - Notification immédiate WebSocket + Email/SMS

**Commandes descendantes (Backend → Nœuds)** :

1. **Configuration** :
   - `apru40/gateway-01/config/node/01` → JSON configuration
   - Modification PIN Bluetooth, MAC whitelist, périodes acquisition

2. **OTA Firmware** :
   - `apru40/gateway-01/ota/node/01` → URL firmware signé
   - Mise à jour firmware à distance

3. **Révocation** :
   - `apru40/gateway-01/revoke/node/01` → Désactivation nœud compromis
   - Gateway rejette paquets ESP-NOW de ce nœud

---

## 3. Spécifications Fonctionnelles

### 3.1 Gestion des Dispositifs

#### 3.1.1 Inventaire (CMDB)

**Fonctionnalité** : Base de données centralisée de tous les dispositifs IoT

**Champs par Nœud** :

| Champ | Type | Description | Exemple |
|-------|------|-------------|---------|
| `node_id` | Integer | ID unique nœud (1-254) | 1 |
| `node_name` | String | Nom convivial | "APRU40-Node-01" |
| `gateway_id` | Integer | Gateway parent | 1 |
| `mac_address` | String | Adresse MAC ESP32 | "AA:BB:CC:DD:EE:FF" |
| `firmware_version` | String | Version firmware | "2.0.1" |
| `last_seen` | Timestamp | Dernier heartbeat | 2026-02-02 14:32:15 |
| `status` | Enum | online/offline/compromised | "online" |
| `tamper_count` | Integer | Nombre tamper détectés | 0 |
| `bt_scanner_mac` | String | MAC scanner autorisé | "12:34:56:78:9A:BC" |
| `bt_pin` | String | PIN Bluetooth actuel | "736428" |
| `deployed_date` | Date | Date déploiement | 2026-01-15 |
| `deployed_by` | String | Opérateur | "Jean Dupont" |
| `location` | String | Emplacement physique | "Atelier A, Machine 3" |
| `seal_number` | String | Numéro scellé sécurité | "#001234" |
| `notes` | Text | Notes maintenance | "Remplacé capteur ADS1119 #2" |

**Champs par Gateway** :

| Champ | Type | Description | Exemple |
|-------|------|-------------|---------|
| `gateway_id` | Integer | ID unique gateway | 1 |
| `gateway_name` | String | Nom convivial | "Gateway-Site-A" |
| `mac_address` | String | Adresse MAC | "11:22:33:44:55:66" |
| `ip_address` | String | IP Ethernet | "192.168.1.100" |
| `mqtt_client_id` | String | Client ID MQTT | "apru40_gw_01" |
| `cert_expiry` | Date | Expiration certificat TLS | 2026-12-31 |
| `firmware_version` | String | Version firmware | "2.0.1" |
| `node_count` | Integer | Nombre nœuds connectés | 28 |
| `uptime` | Duration | Temps fonctionnement | "15d 8h 23m" |
| `last_restart` | Timestamp | Dernier redémarrage | 2026-01-18 06:00 |

**Actions utilisateur** :

- ✅ Lister tous les dispositifs (filtres : status, gateway, location)
- ✅ Recherche (nom, MAC, ID, location)
- ✅ Export CSV/Excel (inventaire complet)
- ✅ Ajout manuel nœud (provisioning)
- ✅ Édition métadonnées (location, notes, seal_number)
- ✅ Historique modifications (audit trail)

**Écran "Inventaire"** :

```
┌─────────────────────────────────────────────────────────────────┐
│ 📋 Inventaire Dispositifs                          [+ Ajouter]   │
├─────────────────────────────────────────────────────────────────┤
│ Filtres: [Tous v] [Gateway: Tous v] [Status: Tous v] [🔍 Search]│
├─────────────────────────────────────────────────────────────────┤
│ ID  │ Nom             │ Gateway │ Status  │ Last Seen        │⚙│
├─────┼─────────────────┼─────────┼─────────┼──────────────────┼─┤
│ 001 │ APRU40-Node-01  │ GW-01   │ 🟢 Online│ 2s ago           │⚙│
│ 002 │ APRU40-Node-02  │ GW-01   │ 🟢 Online│ 5s ago           │⚙│
│ 003 │ APRU40-Node-03  │ GW-01   │ 🔴 Offline│ 3h ago          │⚙│
│ 015 │ APRU40-Node-15  │ GW-02   │ 🟠 Tamper│ 1d ago           │⚙│
├─────┴─────────────────┴─────────┴─────────┴──────────────────┴─┤
│ 📊 Total: 150 dispositifs │ Online: 142 │ Offline: 7 │ Alert: 1│
└─────────────────────────────────────────────────────────────────┘
```

#### 3.1.2 Détails Dispositif

**Écran "Détail Nœud"** (clic sur ligne inventaire) :

```
┌─────────────────────────────────────────────────────────────────┐
│ 📱 APRU40-Node-01                           [Éditer] [Supprimer]│
├─────────────────────────────────────────────────────────────────┤
│ ℹ️ Informations Générales                                       │
│   ID: 001                    MAC: AA:BB:CC:DD:EE:FF             │
│   Gateway: GW-01             Status: 🟢 Online (2s)             │
│   Firmware: 2.0.1            Last Restart: 15d 8h ago           │
│   Location: Atelier A, Machine 3                                │
│   Scellé: #001234            Deployed: 2026-01-15 par Jean D.  │
├─────────────────────────────────────────────────────────────────┤
│ 🔐 Sécurité                                                     │
│   PIN Bluetooth: 736428      [Régénérer PIN]                   │
│   Scanner MAC: 12:34:56:78:9A:BC  [Modifier]                   │
│   Tamper Count: 0            Tamper Auto-Erase: ✅ Activé       │
│   Status: ✅ Non compromis                                      │
├─────────────────────────────────────────────────────────────────┤
│ 📊 Capteurs (Dernière lecture: 2s ago)                         │
│   ADS7128 CH0: 12.34V  CH1: 5.67V  CH2: 3.21A  CH3: 1.23°C    │
│   ADS1119 #1: 1234mV, 5678mV, 9012mV, 3456mV                  │
│   ADS1119 #2: 2345mV, 6789mV, 1234mV, 5678mV                  │
│   TCA9537: GPIO0=HIGH, GPIO1=LOW, GPIO2=HIGH, GPIO3=LOW       │
├─────────────────────────────────────────────────────────────────┤
│ ⚙️ Configuration                                [Envoyer Config]│
│   Période acquisition: 5000ms                                   │
│   Période ESP-NOW TX: 10000ms                                   │
│   Heartbeat: 30000ms                                            │
├─────────────────────────────────────────────────────────────────┤
│ 📜 Historique Événements (7 derniers jours)                    │
│   2026-02-02 14:30 | INFO  | Heartbeat OK                      │
│   2026-02-02 12:15 | WARN  | Connexion BT refusée (AA:BB:...)  │
│   2026-02-01 08:00 | INFO  | Redémarrage programmé             │
│   2026-01-31 18:45 | INFO  | Configuration mise à jour         │
└─────────────────────────────────────────────────────────────────┘
```

**Actions disponibles** :

| Action | Description | Confirmation |
|--------|-------------|--------------|
| **Éditer** | Modifier métadonnées (location, notes) | Non |
| **Régénérer PIN** | Générer nouveau PIN Bluetooth aléatoire | ⚠️ Oui (re-pairing requis) |
| **Modifier Scanner MAC** | Changer scanner autorisé | ⚠️ Oui |
| **Envoyer Config** | Pousser configuration via MQTT | Non |
| **OTA Update** | Mettre à jour firmware | ⚠️ Oui |
| **Désactiver** | Désactiver nœud (révocation) | ⚠️ Oui |
| **Révoquer** | Marquer comme compromis (blacklist) | 🔴 Oui |
| **Supprimer** | Retirer de l'inventaire | 🔴 Oui |

### 3.2 Monitoring Temps Réel

#### 3.2.1 Tableau de Bord Principal

**Écran "Dashboard"** :

```
┌─────────────────────────────────────────────────────────────────┐
│ 📊 Dashboard APRU40                          Rafraîchi: 2s ago  │
├─────────────────────────────────────────────────────────────────┤
│ ┌────────────────┐ ┌────────────────┐ ┌────────────────┐       │
│ │ 🟢 Online      │ │ 🔴 Offline     │ │ 🟠 Alertes     │       │
│ │   142 / 150    │ │     7          │ │     1          │       │
│ │   94.7%        │ │   4.7%         │ │   0.7%         │       │
│ └────────────────┘ └────────────────┘ └────────────────┘       │
├─────────────────────────────────────────────────────────────────┤
│ 📈 Graphique Connectivité (24h)                                │
│   │ 100%                                                        │
│   │  95%  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓                              │
│   │  90%  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓                              │
│   │  85%  ░░░░░░░░░░░░░░░░░░░░░░░                              │
│   └──────────────────────────────────────► Temps              │
│       00h    06h    12h    18h    24h                          │
├─────────────────────────────────────────────────────────────────┤
│ 🚨 Alertes Actives                                [Voir Tout]  │
│   ⚠️ Node-15: Tamper détecté (1d ago) - [Voir] [Résoudre]     │
│   ⚠️ Node-03: Offline depuis 3h - [Voir] [Diagnostiquer]      │
├─────────────────────────────────────────────────────────────────┤
│ 🔌 Gateways (5)                                                │
│   GW-01: 🟢 Online (28 nodes) | GW-02: 🟢 Online (30 nodes)   │
│   GW-03: 🟢 Online (25 nodes) | GW-04: 🟢 Online (29 nodes)   │
│   GW-05: 🟢 Online (31 nodes)                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Widgets temps réel** (WebSocket) :

1. **Compteurs KPI** :
   - Total dispositifs / Online / Offline / Alertes
   - Mise à jour toutes les 2 secondes

2. **Carte réseau** (optionnel) :
   - Visualisation graphique gateways + nœuds (nodes.js D3.js)
   - Couleur par statut (vert/rouge/orange)
   - Clic sur nœud → Popup détails

3. **Timeline événements** :
   - Flux temps réel des événements (connexion, déconnexion, alertes)
   - Filtrable par type, gateway, nœud

4. **Alertes actives** :
   - Liste alertes non résolues
   - Lien vers détail + actions rapides

#### 3.2.2 Monitoring Capteurs

**Écran "Capteurs Temps Réel"** :

```
┌─────────────────────────────────────────────────────────────────┐
│ 📊 Monitoring Capteurs - Node-01              Rafraîchi: 2s ago │
├─────────────────────────────────────────────────────────────────┤
│ 🔋 ADS7128 (8 canaux - 12-bit)                                 │
│   CH0: ▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░ 12.34V (Tension batterie)         │
│   CH1: ▓▓▓▓░░░░░░░░░░░░░░░░  5.67V (Tension moteur)           │
│   CH2: ▓▓▓▓▓▓▓░░░░░░░░░░░░░  3.21A (Courant moteur)           │
│   CH3: ▓▓▓░░░░░░░░░░░░░░░░░  1.23°C (Température)             │
│   CH4-7: [Inactifs]                                            │
├─────────────────────────────────────────────────────────────────┤
│ 📈 Historique CH0 (24h)                          [Export CSV]  │
│   │ 15V                                                         │
│   │ 12V  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓                                  │
│   │  9V  ░░░░░░░░░░░░░░░░░░░░                                  │
│   │  6V  ░░░░░░░░░░░░░░░░░░░░                                  │
│   └──────────────────────────────────────► Temps              │
│       00h    06h    12h    18h    24h                          │
└─────────────────────────────────────────────────────────────────┘
```

**Fonctionnalités** :

- ✅ Graphiques temps réel (Chart.js, Plotly.js)
- ✅ Multi-nœuds (comparaison jusqu'à 4 nœuds)
- ✅ Seuils d'alerte configurables (ex: CH0 < 10V → Alerte batterie faible)
- ✅ Export données (CSV, JSON) pour analyse externe
- ✅ Zoom temporel (1h, 6h, 24h, 7j, 30j)

### 3.3 Gestion des Alertes

#### 3.3.1 Types d'Alertes

| Type | Priorité | Description | Actions |
|------|----------|-------------|---------|
| **Tamper** | 🔴 Critique | Boîtier ouvert, NVS effacé | Audit forensique, reflash |
| **BT Unauthorized** | 🟠 Haute | Tentative connexion BT non autorisée | Investigation, changement PIN |
| **Node Offline** | 🟡 Moyenne | Nœud sans heartbeat > 60s | Diagnostic réseau, redémarrage |
| **Gateway Offline** | 🔴 Critique | Gateway MQTT déconnecté | Vérification infrastructure |
| **Cert Expiring** | 🟡 Moyenne | Certificat TLS expire < 30j | Renouvellement certificat |
| **Battery Low** | 🟡 Moyenne | Tension batterie < seuil | Remplacement batterie |
| **Sensor Anomaly** | 🟡 Moyenne | Valeur capteur hors plage | Calibration, remplacement |

#### 3.3.2 Écran Alertes

```
┌─────────────────────────────────────────────────────────────────┐
│ 🚨 Alertes et Incidents                         [+ Nouvelle]    │
├─────────────────────────────────────────────────────────────────┤
│ Filtres: [Type: Tous v] [Priorité: Tous v] [Status: Actif v]   │
├─────────────────────────────────────────────────────────────────┤
│ 🔴 CRITIQUE | Node-15 | Tamper détecté              | 1d ago    │
│    Status: 🟠 En cours | Assigné: Jean D. | [Détails] [Résoudre]│
├─────────────────────────────────────────────────────────────────┤
│ 🟠 HAUTE    | Node-03 | BT Unauthorized (AA:BB:..) | 2h ago    │
│    Status: 🆕 Nouveau  | Non assigné      | [Détails] [Assigner]│
├─────────────────────────────────────────────────────────────────┤
│ 🟡 MOYENNE  | GW-02   | Certificat expire 25j      | 1w ago    │
│    Status: 🟠 En cours | Assigné: Marie L.| [Détails] [Résoudre]│
└─────────────────────────────────────────────────────────────────┘
```

**Workflow alerte** :

1. **Création automatique** (trigger depuis MQTT)
2. **Notification** (WebSocket push + Email/SMS optionnel)
3. **Assignation** (à un opérateur)
4. **Investigation** (ajout notes, actions)
5. **Résolution** (fermeture avec commentaire)
6. **Archivage** (historique 1 an minimum pour NIS2)

**Champs alerte** :

```json
{
  "alert_id": 1234,
  "type": "tamper",
  "priority": "critical",
  "status": "in_progress",
  "device_type": "node",
  "device_id": 15,
  "device_name": "APRU40-Node-15",
  "gateway_id": 2,
  "timestamp": "2026-02-01T14:32:15Z",
  "description": "Tamper détecté, NVS effacé, redémarrage effectué",
  "assigned_to": "jean.dupont@example.com",
  "notes": [
    {"by": "jean.dupont", "date": "2026-02-01T15:00", "text": "Inspection physique programmée"},
    {"by": "jean.dupont", "date": "2026-02-01T16:30", "text": "Scellé intact, fausse alerte"}
  ],
  "resolved_at": null,
  "resolved_by": null,
  "resolution": null
}
```

### 3.4 Configuration et Provisioning

#### 3.4.1 Provisioning Nouveau Nœud

**Assistant "Ajouter Nœud"** (wizard 5 étapes) :

```
┌─────────────────────────────────────────────────────────────────┐
│ 🆕 Ajouter Nouveau Nœud                        Étape 1/5        │
├─────────────────────────────────────────────────────────────────┤
│ ℹ️ Informations de Base                                         │
│   Node ID: [___] (1-254, auto-suggéré: 32)                     │
│   Nom: [APRU40-Node-___]                                        │
│   Gateway: [Sélectionner v] → GW-01 (28 nodes)                 │
│   Location: [___________________]                               │
│                                                                 │
│                               [Annuler]  [Suivant >]            │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 🆕 Ajouter Nouveau Nœud                        Étape 2/5        │
├─────────────────────────────────────────────────────────────────┤
│ 🔐 Sécurité Bluetooth                                           │
│   PIN: [⚡ Générer Aléatoire] ou [_______] (6 chiffres)         │
│   PIN généré: 847392                                            │
│                                                                 │
│   MAC Scanner Zebra: [__:__:__:__:__:__]                        │
│   ℹ️ Obtenir depuis: Scanner > Settings > About > BT Address   │
│                                                                 │
│                               [< Retour]  [Suivant >]           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 🆕 Ajouter Nouveau Nœud                        Étape 3/5        │
├─────────────────────────────────────────────────────────────────┤
│ ⚙️ Configuration Capteurs                                       │
│   [✅] ADS7128 (8 canaux 12-bit)                                │
│   [✅] ADS1119 #1 (4 canaux 16-bit)                             │
│   [✅] ADS1119 #2 (4 canaux 16-bit)                             │
│   [✅] TCA9537 (4 GPIO)                                          │
│                                                                 │
│   Période acquisition: [5000] ms                                │
│   Période ESP-NOW TX: [10000] ms                                │
│                                                                 │
│                               [< Retour]  [Suivant >]           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 🆕 Ajouter Nouveau Nœud                        Étape 4/5        │
├─────────────────────────────────────────────────────────────────┤
│ 🔒 Sécurité Tamper                                              │
│   GPIO Tamper: [GPIO_NUM_34 v]                                 │
│   Active Low: [✅] Oui  [ ] Non                                 │
│   Auto-Erase NVS: [ ] DEV  [✅] PRODUCTION                      │
│   Auto-Restart: [ ] DEV  [✅] PRODUCTION                        │
│                                                                 │
│   ⚠️ Mode PRODUCTION : Effacement NVS en cas d'intrusion       │
│                                                                 │
│                               [< Retour]  [Suivant >]           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 🆕 Ajouter Nouveau Nœud                        Étape 5/5        │
├─────────────────────────────────────────────────────────────────┤
│ 📋 Récapitulatif                                                │
│   Node ID: 32                                                   │
│   Nom: APRU40-Node-32                                           │
│   Gateway: GW-01                                                │
│   PIN Bluetooth: 847392                                         │
│   Scanner MAC: 12:34:56:78:9A:BC                                │
│                                                                 │
│ 📦 Prochaines Étapes :                                          │
│   1. Générer node_config.h personnalisé [Télécharger]          │
│   2. Compiler firmware avec PlatformIO                          │
│   3. Flasher ESP32 via USB                                      │
│   4. Déployer physiquement sur site                             │
│   5. Pairing scanner Bluetooth (PIN: 847392)                    │
│                                                                 │
│                         [< Retour]  [Créer et Télécharger]     │
└─────────────────────────────────────────────────────────────────┘
```

**Génération automatique** :

L'interface génère automatiquement un fichier `node_config.h` personnalisé :

```c
// node_config.h - Généré automatiquement par APRU40 Admin
// Node ID: 32 | Nom: APRU40-Node-32 | Gateway: GW-01
// Date: 2026-02-02 14:45:00 | Opérateur: jean.dupont@example.com

#define NODE_MODE       MODE_NODE
#define NODE_ID         32
#define NODE_NAME       "APRU40-Node-32"

// Sécurité Bluetooth
#define BT_PIN_CODE     "847392"
#define BT_SCANNER_MAC_WHITELIST  {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}

// Configuration Tamper
#define TAMPER_GPIO                 GPIO_NUM_34
#define TAMPER_ACTIVE_LOW           true
#define TAMPER_AUTO_ERASE_NVS       true
#define TAMPER_AUTO_RESTART         true

// ... reste de la configuration
```

#### 3.4.2 Configuration OTA (Over-The-Air)

**Fonctionnalité** : Mise à jour configuration à distance (sans reflash)

**Paramètres modifiables OTA** :

| Paramètre | Description | Restart requis |
|-----------|-------------|----------------|
| `acquisition_period` | Période acquisition capteurs | Non |
| `espnow_tx_period` | Période envoi ESP-NOW | Non |
| `heartbeat_period` | Période heartbeat | Non |
| `bt_pin` | PIN Bluetooth | Oui (re-pairing) |
| `bt_scanner_mac` | MAC scanner autorisé | Oui |
| `tamper_auto_erase` | Effacement auto NVS | Oui |

**Flux mise à jour** :

1. **Frontend** : Utilisateur modifie config dans interface
2. **Backend** : Validation config + génération JSON
3. **MQTT Publish** : `apru40/gateway-01/config/node/32` → JSON config
4. **Nœud** : Réception MQTT, écriture NVS, restart (si requis)
5. **Confirmation** : Nœud publie `apru40/gateway-01/config/node/32/ack`
6. **Frontend** : Notification WebSocket "Config appliquée ✅"

### 3.5 Gestion des Certificats TLS

**Écran "Certificats"** :

```
┌─────────────────────────────────────────────────────────────────┐
│ 🔐 Gestion Certificats TLS                    [+ Nouveau Cert]  │
├─────────────────────────────────────────────────────────────────┤
│ Gateway  │ Type      │ Expire       │ Status    │ Actions       │
├──────────┼───────────┼──────────────┼───────────┼───────────────┤
│ GW-01    │ Client    │ 2026-12-31   │ 🟢 Valide │ [Renouveler]  │
│ GW-01    │ CA Root   │ 2027-12-31   │ 🟢 Valide │ [Voir]        │
│ GW-02    │ Client    │ 2026-03-15   │ 🟠 25j    │ [Renouveler!] │
│ GW-02    │ CA Root   │ 2027-12-31   │ 🟢 Valide │ [Voir]        │
│ Broker   │ Serveur   │ 2027-06-30   │ 🟢 Valide │ [Voir]        │
└─────────────────────────────────────────────────────────────────┘
```

**Fonctionnalités** :

- ✅ Suivi expirations (alertes 30j, 15j, 7j avant)
- ✅ Génération CSR (Certificate Signing Request)
- ✅ Upload nouveau certificat (.pem)
- ✅ Déploiement OTA vers gateway (via carte SD virtuelle)
- ✅ Validation certificat (chaîne de confiance)
- ✅ Révocation certificat compromis (CRL)

---

## 4. Spécifications Techniques

### 4.1 Backend API

#### 4.1.1 Stack Recommandé

**Option 1 : Node.js + Express**
- **Avantages** : Écosystème riche, async natif, WebSocket facile, JSON-first
- **Librairies** :
  - `express` : API REST
  - `socket.io` : WebSocket temps réel
  - `mqtt` : Client MQTT
  - `mongoose` : MongoDB ORM
  - `passport` : Authentification
  - `joi` : Validation données

**Option 2 : Python + FastAPI**
- **Avantages** : Performance, typage (Pydantic), documentation auto (Swagger)
- **Librairies** :
  - `fastapi` : API REST moderne
  - `uvicorn` : Serveur ASGI
  - `websockets` : WebSocket
  - `paho-mqtt` : Client MQTT
  - `sqlalchemy` : PostgreSQL ORM
  - `passlib` : Hash passwords

**Option 3 : Go + Gin**
- **Avantages** : Performance maximale, typage fort, concurrence native
- **Librairies** :
  - `gin` : Framework web
  - `gorilla/websocket` : WebSocket
  - `paho.mqtt.golang` : Client MQTT
  - `gorm` : ORM
  - `jwt-go` : JWT auth

#### 4.1.2 API REST Endpoints

**Authentification** :

```
POST   /api/v1/auth/login          # Login (email + password)
POST   /api/v1/auth/logout         # Logout
POST   /api/v1/auth/refresh        # Refresh token JWT
GET    /api/v1/auth/me             # User profile
```

**Dispositifs** :

```
GET    /api/v1/devices             # Liste tous dispositifs (filtres query params)
POST   /api/v1/devices             # Créer nouveau dispositif
GET    /api/v1/devices/:id         # Détails dispositif
PUT    /api/v1/devices/:id         # Mettre à jour métadonnées
DELETE /api/v1/devices/:id         # Supprimer dispositif

GET    /api/v1/devices/:id/sensors # Dernières valeurs capteurs
GET    /api/v1/devices/:id/history # Historique capteurs (range temporel)
GET    /api/v1/devices/:id/events  # Historique événements
```

**Configuration OTA** :

```
POST   /api/v1/devices/:id/config  # Envoyer configuration (JSON)
GET    /api/v1/devices/:id/config  # Récupérer config actuelle
POST   /api/v1/devices/:id/reboot  # Redémarrer dispositif
POST   /api/v1/devices/:id/revoke  # Révoquer dispositif (blacklist)
```

**Gateways** :

```
GET    /api/v1/gateways            # Liste gateways
GET    /api/v1/gateways/:id        # Détails gateway
GET    /api/v1/gateways/:id/nodes  # Nœuds connectés à ce gateway
```

**Alertes** :

```
GET    /api/v1/alerts              # Liste alertes (filtres)
POST   /api/v1/alerts              # Créer alerte manuelle
GET    /api/v1/alerts/:id          # Détails alerte
PUT    /api/v1/alerts/:id          # Mettre à jour alerte (assignation, notes)
POST   /api/v1/alerts/:id/resolve  # Résoudre alerte
DELETE /api/v1/alerts/:id          # Supprimer alerte (archivage)
```

**Certificats** :

```
GET    /api/v1/certificates        # Liste certificats TLS
POST   /api/v1/certificates        # Upload nouveau certificat
GET    /api/v1/certificates/:id    # Détails certificat
DELETE /api/v1/certificates/:id    # Révoquer certificat
```

**Statistiques** :

```
GET    /api/v1/stats/dashboard     # KPI dashboard (online/offline/alertes)
GET    /api/v1/stats/connectivity  # Historique connectivité (graphique)
GET    /api/v1/stats/sensors       # Statistiques capteurs (min/max/avg)
```

**Exports** :

```
GET    /api/v1/export/devices      # Export CSV inventaire
GET    /api/v1/export/sensors      # Export CSV données capteurs
GET    /api/v1/export/alerts       # Export CSV alertes (NIS2)
GET    /api/v1/export/audit-trail  # Export audit trail (NIS2)
```

#### 4.1.3 WebSocket Events

**Client → Server** :

```javascript
// Abonnement temps réel
socket.emit('subscribe', {
  type: 'sensors',
  device_id: 1
});

// Désabonnement
socket.emit('unsubscribe', {
  type: 'sensors',
  device_id: 1
});
```

**Server → Client** :

```javascript
// Nouveau heartbeat
socket.on('device:heartbeat', (data) => {
  // {device_id: 1, timestamp: '2026-02-02T14:32:15Z', status: 'online'}
});

// Nouvelle donnée capteur
socket.on('device:sensor_data', (data) => {
  // {device_id: 1, sensors: {...}, timestamp: '...'}
});

// Nouvelle alerte
socket.on('alert:new', (data) => {
  // {alert_id: 1234, type: 'tamper', device_id: 15, ...}
});

// Changement statut dispositif
socket.on('device:status_change', (data) => {
  // {device_id: 1, old_status: 'online', new_status: 'offline'}
});

// Configuration appliquée
socket.on('device:config_applied', (data) => {
  // {device_id: 1, config: {...}, success: true}
});
```

#### 4.1.4 Intégration MQTT

**Abonnements Backend** :

```javascript
// Données capteurs (tous nœuds, toutes gateways)
mqtt.subscribe('apru40/+/node/+/data');

// Status/heartbeat
mqtt.subscribe('apru40/+/node/+/status');

// Alertes sécurité
mqtt.subscribe('apru40/alert/#');

// ACK configuration
mqtt.subscribe('apru40/+/config/node/+/ack');
```

**Publications Backend** :

```javascript
// Configuration OTA
mqtt.publish('apru40/gateway-01/config/node/01', JSON.stringify(config));

// OTA firmware
mqtt.publish('apru40/gateway-01/ota/node/01', {url: 'https://...', hash: '...'});

// Révocation nœud
mqtt.publish('apru40/gateway-01/revoke/node/01', {reason: 'compromised'});
```

### 4.2 Frontend Web

#### 4.2.1 Stack Recommandé

**Option 1 : React.js + Material-UI**
- **Framework** : React 18+, TypeScript
- **UI** : Material-UI (MUI), Tailwind CSS
- **State** : Redux Toolkit, React Query
- **Routing** : React Router v6
- **WebSocket** : Socket.io-client
- **Charts** : Chart.js, Recharts
- **Forms** : React Hook Form, Yup validation

**Option 2 : Vue.js + Vuetify**
- **Framework** : Vue 3 + Composition API, TypeScript
- **UI** : Vuetify 3
- **State** : Pinia
- **Routing** : Vue Router
- **WebSocket** : Socket.io-client
- **Charts** : ApexCharts

**Option 3 : Angular + Angular Material**
- **Framework** : Angular 16+, TypeScript
- **UI** : Angular Material
- **State** : NgRx
- **WebSocket** : Socket.io-client
- **Charts** : ng2-charts

#### 4.2.2 Architecture Frontend

```
src/
├── components/           # Composants réutilisables
│   ├── DeviceCard.tsx
│   ├── AlertBadge.tsx
│   ├── SensorChart.tsx
│   └── ...
├── pages/               # Pages principales
│   ├── Dashboard.tsx
│   ├── Devices.tsx
│   ├── DeviceDetail.tsx
│   ├── Alerts.tsx
│   ├── Certificates.tsx
│   └── Settings.tsx
├── services/            # API clients
│   ├── api.ts          # REST API client (axios)
│   ├── websocket.ts    # WebSocket client
│   └── auth.ts         # Authentification
├── store/               # State management
│   ├── devices.ts
│   ├── alerts.ts
│   └── user.ts
├── types/               # TypeScript types
│   ├── Device.ts
│   ├── Alert.ts
│   └── Sensor.ts
└── utils/               # Utilitaires
    ├── formatters.ts
    ├── validators.ts
    └── constants.ts
```

#### 4.2.3 Responsive Design

**Breakpoints** :

- **Mobile** : < 640px (smartphone portrait)
- **Tablet** : 640px - 1024px (tablet, smartphone landscape)
- **Desktop** : > 1024px (ordinateur, écran large)

**Adaptations** :

| Écran | Layout | Navigation | Graphiques |
|-------|--------|------------|------------|
| Mobile | 1 colonne | Hamburger menu | Graphiques simplifiés |
| Tablet | 2 colonnes | Sidebar collapsible | Graphiques adaptés |
| Desktop | 3-4 colonnes | Sidebar fixe | Graphiques complets |

**Fonctionnalités mobiles** :

- ✅ Touch-friendly (boutons 44×44px minimum)
- ✅ Swipe gestures (navigation, refresh)
- ✅ Notifications push (alertes critiques)
- ⚠️ Mode offline partiel (cache read-only)

### 4.3 Base de Données

#### 4.3.1 Modèle de Données

**Collections/Tables** :

**devices** (nœuds + gateways)

```sql
CREATE TABLE devices (
  id SERIAL PRIMARY KEY,
  device_type VARCHAR(20) NOT NULL, -- 'node' | 'gateway'
  node_id INTEGER,
  gateway_id INTEGER,
  mac_address VARCHAR(17) UNIQUE NOT NULL,
  name VARCHAR(100) NOT NULL,
  firmware_version VARCHAR(20),
  status VARCHAR(20) DEFAULT 'offline', -- 'online' | 'offline' | 'compromised'
  last_seen TIMESTAMP,
  location TEXT,
  deployed_date DATE,
  deployed_by VARCHAR(100),
  seal_number VARCHAR(50),
  bt_pin VARCHAR(6),
  bt_scanner_mac VARCHAR(17),
  tamper_count INTEGER DEFAULT 0,
  notes TEXT,
  config JSONB, -- Configuration complète
  created_at TIMESTAMP DEFAULT NOW(),
  updated_at TIMESTAMP DEFAULT NOW()
);
```

**sensor_data** (données capteurs)

```sql
CREATE TABLE sensor_data (
  id BIGSERIAL PRIMARY KEY,
  device_id INTEGER REFERENCES devices(id),
  timestamp TIMESTAMP NOT NULL,
  sensor_type VARCHAR(50), -- 'ads7128' | 'ads1119_1' | 'ads1119_2' | 'tca9537'
  channel INTEGER,
  value FLOAT,
  unit VARCHAR(20),
  INDEX idx_device_timestamp (device_id, timestamp DESC)
);
```

**alerts** (alertes sécurité)

```sql
CREATE TABLE alerts (
  id SERIAL PRIMARY KEY,
  alert_type VARCHAR(50) NOT NULL, -- 'tamper' | 'bt_unauthorized' | 'offline' | ...
  priority VARCHAR(20) NOT NULL, -- 'critical' | 'high' | 'medium' | 'low'
  status VARCHAR(20) DEFAULT 'new', -- 'new' | 'in_progress' | 'resolved' | 'archived'
  device_id INTEGER REFERENCES devices(id),
  gateway_id INTEGER,
  timestamp TIMESTAMP NOT NULL,
  description TEXT,
  assigned_to VARCHAR(100),
  resolved_at TIMESTAMP,
  resolved_by VARCHAR(100),
  resolution TEXT,
  metadata JSONB, -- Données spécifiques (MAC unauthorized, etc.)
  created_at TIMESTAMP DEFAULT NOW()
);
```

**certificates** (certificats TLS)

```sql
CREATE TABLE certificates (
  id SERIAL PRIMARY KEY,
  device_id INTEGER REFERENCES devices(id),
  cert_type VARCHAR(20), -- 'client' | 'server' | 'ca_root'
  subject VARCHAR(255),
  issuer VARCHAR(255),
  serial_number VARCHAR(100),
  not_before DATE,
  not_after DATE,
  status VARCHAR(20) DEFAULT 'valid', -- 'valid' | 'expiring' | 'expired' | 'revoked'
  cert_pem TEXT,
  created_at TIMESTAMP DEFAULT NOW()
);
```

**audit_trail** (traçabilité NIS2)

```sql
CREATE TABLE audit_trail (
  id BIGSERIAL PRIMARY KEY,
  timestamp TIMESTAMP NOT NULL,
  user_email VARCHAR(100),
  action VARCHAR(100), -- 'device_created' | 'config_updated' | 'alert_resolved' | ...
  resource_type VARCHAR(50), -- 'device' | 'alert' | 'certificate' | ...
  resource_id INTEGER,
  details JSONB,
  ip_address VARCHAR(45)
);
```

**users** (utilisateurs admin)

```sql
CREATE TABLE users (
  id SERIAL PRIMARY KEY,
  email VARCHAR(100) UNIQUE NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  full_name VARCHAR(100),
  role VARCHAR(20) DEFAULT 'operator', -- 'admin' | 'operator' | 'viewer'
  last_login TIMESTAMP,
  created_at TIMESTAMP DEFAULT NOW()
);
```

#### 4.3.2 Indexation

**Performances requises** :

| Requête | Temps cible | Index |
|---------|-------------|-------|
| Liste devices (100+) | < 100ms | `idx_device_status`, `idx_device_gateway` |
| Données capteurs (1 nœud, 24h) | < 200ms | `idx_sensor_device_timestamp` |
| Alertes actives (filtres) | < 50ms | `idx_alert_status`, `idx_alert_device` |
| Audit trail (recherche) | < 300ms | `idx_audit_user_timestamp` |

#### 4.3.3 Rétention Données

**Politique de rétention** :

| Donnée | Rétention | Archivage | Raison |
|--------|-----------|-----------|--------|
| Sensor data (raw) | 30 jours | → S3/GCS cold storage | Volume élevé |
| Sensor data (agrégé) | 1 an | → S3/GCS | Conformité |
| Alertes | 1 an | → S3/GCS | NIS2 (audit trail) |
| Audit trail | 1 an | → S3/GCS | NIS2 (obligation légale) |
| Devices metadata | ∞ (permanent) | - | Inventaire |

---

## 5. Sécurité et Conformité

### 5.1 Authentification et Autorisation

#### 5.1.1 Authentification

**Mécanisme** : JWT (JSON Web Token)

**Flux login** :

1. Frontend → `POST /api/v1/auth/login` {email, password}
2. Backend → Validation (bcrypt hash)
3. Backend → Génération JWT (expire 1h)
4. Backend → Response {access_token, refresh_token}
5. Frontend → Stockage token (localStorage)
6. Frontend → Toutes requêtes : `Authorization: Bearer <token>`

**Refresh token** :

- Durée : 7 jours
- Endpoint : `POST /api/v1/auth/refresh`
- Rotation : Nouveau refresh token à chaque refresh

#### 5.1.2 Autorisation (RBAC)

**Rôles utilisateur** :

| Rôle | Permissions | Cas d'usage |
|------|-------------|-------------|
| **Admin** | Lecture + Écriture + Suppression (tout) | Administrateur système |
| **Operator** | Lecture + Écriture (devices, alertes) | Opérateur terrain |
| **Viewer** | Lecture seule (dashboard, devices) | Management, audit |

**Matrice de permissions** :

| Ressource | Admin | Operator | Viewer |
|-----------|-------|----------|--------|
| Dashboard | ✅ RW | ✅ RW | ✅ R |
| Devices (list) | ✅ RW | ✅ RW | ✅ R |
| Devices (create/delete) | ✅ | ❌ | ❌ |
| Config OTA | ✅ | ✅ | ❌ |
| Alertes (resolve) | ✅ | ✅ | ❌ |
| Certificats | ✅ | ❌ | ❌ |
| Users management | ✅ | ❌ | ❌ |

### 5.2 Sécurité Application

**Bonnes pratiques** :

| Mesure | Implémentation |
|--------|----------------|
| **HTTPS obligatoire** | Certificat TLS Let's Encrypt, redirection HTTP → HTTPS |
| **CORS** | Whitelist domaines autorisés |
| **Rate limiting** | 100 req/min par IP (authentification), 1000 req/min (API) |
| **Input validation** | Sanitization (SQL injection, XSS) avec Joi/Yup |
| **Password policy** | Min 12 caractères, majuscules, chiffres, symboles |
| **2FA (optionnel)** | TOTP (Google Authenticator) pour admins |
| **Session timeout** | 1h inactivité → Auto logout |
| **Audit logging** | Toutes actions critiques (création, suppression, config) |

### 5.3 Conformité NIS2

**Exigences couvertes** :

| Article NIS2 | Exigence | Implémentation |
|--------------|----------|----------------|
| **Art. 21.1.a** | Gestion des risques | Dashboard alertes, monitoring |
| **Art. 21.1.b** | Gestion des incidents | Module alertes + workflow résolution |
| **Art. 21.1.j** | Gestion des actifs | Inventaire CMDB complet |
| **Art. 21.2.c** | Authentification forte | JWT + RBAC + 2FA optionnel |
| **Art. 22** | Notification incidents | Export CSV alertes, rapports |
| **Art. 23** | Audit trail | Table audit_trail (1 an minimum) |

**Rapports NIS2** :

```
GET /api/v1/reports/nis2/incidents?start=2026-01-01&end=2026-12-31
→ Export CSV : Date, Type, Device, Priority, Status, Resolved By, Duration
```

---

## 6. Interfaces Utilisateur

### 6.1 Wireframes Principaux

*(Voir fichier annexe `WIREFRAMES.pdf` pour détails visuels)*

**Pages obligatoires** :

1. **Login** : Authentification
2. **Dashboard** : Vue d'ensemble temps réel
3. **Devices** : Liste inventaire + recherche
4. **Device Detail** : Détails nœud/gateway + graphiques capteurs
5. **Alerts** : Liste alertes + workflow résolution
6. **Configuration** : Assistant provisioning + OTA
7. **Certificates** : Gestion certificats TLS
8. **Users** : Gestion utilisateurs (admin uniquement)
9. **Settings** : Paramètres généraux

### 6.2 UX Critères

**Performance** :

- ✅ Temps chargement initial < 3s (4G)
- ✅ Interactions < 100ms (boutons, liens)
- ✅ WebSocket latency < 500ms (alertes temps réel)

**Accessibilité** :

- ✅ WCAG 2.1 AA (contraste, navigation clavier)
- ✅ Labels ARIA (screen readers)
- ✅ Focus visible (navigation clavier)

**Ergonomie** :

- ✅ Navigation intuitive (max 3 clics pour action)
- ✅ Confirmation actions critiques (suppression, révocation)
- ✅ Messages erreur explicites + suggestions
- ✅ Raccourcis clavier (power users)

---

## 7. API et Intégrations

### 7.1 Documentation API

**Outil** : Swagger / OpenAPI 3.0

**Accès** : `https://admin.apru40.com/api/docs`

**Fonctionnalités** :

- ✅ Documentation interactive (Swagger UI)
- ✅ Exemples requêtes/réponses
- ✅ Authentification test (Bearer token)
- ✅ Export JSON/YAML

### 7.2 Webhooks (Phase 2)

**Événements configurables** :

| Événement | Payload | Cas d'usage |
|-----------|---------|-------------|
| `alert.created` | {alert_id, type, device, ...} | Notification Slack/Teams |
| `device.offline` | {device_id, last_seen} | Monitoring externe |
| `certificate.expiring` | {cert_id, expire_in_days} | Alerte équipe infra |

**Configuration** :

```json
{
  "url": "https://hooks.slack.com/services/...",
  "events": ["alert.created"],
  "secret": "webhook_secret_key"
}
```

### 7.3 Intégrations Tierces (Phase 3)

**Plateformes envisagées** :

- **Grafana** : Tableaux de bord avancés (datasource API)
- **Prometheus** : Métriques temps réel (endpoint `/metrics`)
- **PagerDuty** : Escalade alertes critiques
- **Jira** : Création tickets incidents automatique
- **ERP/SAP** : Synchronisation inventaire (API bidirectionnelle)

---

## 8. Stack Technologique

### 8.1 Stack Recommandé (Production)

**Frontend** :

- **Framework** : React 18 + TypeScript
- **UI** : Material-UI (MUI) + Tailwind CSS
- **State** : Redux Toolkit + React Query
- **Charts** : Chart.js + Recharts
- **Build** : Vite (dev), Webpack (prod)

**Backend** :

- **Runtime** : Node.js 20 LTS
- **Framework** : Express.js 4.x
- **WebSocket** : Socket.io 4.x
- **MQTT** : mqtt.js 5.x
- **Validation** : Joi
- **Auth** : jsonwebtoken, bcrypt

**Base de données** :

- **Principale** : PostgreSQL 16 (données structurées)
- **Cache** : Redis 7 (sessions, rate limiting)
- **Time-series** : TimescaleDB (extension PostgreSQL pour sensor_data)

**Infrastructure** :

- **Serveur** : Ubuntu 22.04 LTS
- **Reverse proxy** : Nginx 1.24
- **Certificat** : Let's Encrypt (Certbot)
- **Monitoring** : PM2 (Node.js), Prometheus, Grafana
- **Logs** : Winston (app), ELK Stack (centralisé)

### 8.2 Environnements

| Env | Usage | URL | Base données |
|-----|-------|-----|--------------|
| **Development** | Dev local | localhost:3000 | PostgreSQL local |
| **Staging** | Tests pré-prod | staging.apru40.com | PostgreSQL staging |
| **Production** | Utilisateurs finaux | admin.apru40.com | PostgreSQL prod (HA) |

### 8.3 Déploiement

**Option 1 : Serveur Dédié (VPS)**

- Provider : OVH, Hetzner, DigitalOcean
- Specs : 8 GB RAM, 4 vCPU, 200 GB SSD
- Coût : ~30-50€/mois

**Option 2 : Docker + Docker Compose**

```yaml
version: '3.8'
services:
  frontend:
    image: apru40-admin-frontend:latest
    ports: ["80:80", "443:443"]
  backend:
    image: apru40-admin-backend:latest
    ports: ["3001:3001"]
    environment:
      - DATABASE_URL=postgresql://...
      - MQTT_BROKER_URL=mqtt://...
  postgres:
    image: postgres:16
    volumes: [./data:/var/lib/postgresql/data]
  redis:
    image: redis:7
```

**Option 3 : Cloud (AWS, GCP, Azure)**

- Frontend : S3 + CloudFront (AWS) ou Cloud Storage + CDN (GCP)
- Backend : ECS Fargate (AWS) ou Cloud Run (GCP)
- Base données : RDS PostgreSQL (AWS) ou Cloud SQL (GCP)
- Coût : ~100-200€/mois (selon trafic)

---

## 9. Planning et Phases

### 9.1 Phase 1 : MVP (2 mois)

**Objectif** : Interface fonctionnelle avec fonctionnalités essentielles

**Livrables** :

- ✅ Frontend : Dashboard, Devices, Device Detail
- ✅ Backend : API REST complète, WebSocket basique
- ✅ Base données : Schema complet, migrations
- ✅ Authentification : JWT + RBAC (Admin, Operator)
- ✅ Intégration MQTT : Lecture données capteurs + heartbeat
- ✅ Monitoring : Statut online/offline temps réel

**Planning détaillé** :

| Semaine | Tâches | Responsable |
|---------|--------|-------------|
| S1 | Setup projet, architecture, database schema | Dev Backend |
| S2 | API REST devices (CRUD) + authentification | Dev Backend |
| S3 | Frontend setup, pages Dashboard + Devices | Dev Frontend |
| S4 | Intégration MQTT (subscriber) | Dev Backend |
| S5 | WebSocket temps réel (heartbeat) | Dev Backend + Frontend |
| S6 | Page Device Detail + graphiques capteurs | Dev Frontend |
| S7 | Tests e2e, debug, optimisations | Full stack |
| S8 | Déploiement staging, recette utilisateur | DevOps + Product Owner |

**Budget MVP** : 40 jours × 2 devs = 80 j·h (~24k€ à 300€/j)

### 9.2 Phase 2 : Fonctionnalités Avancées (2 mois)

**Objectif** : Ajout gestion alertes, configuration OTA, certificats

**Livrables** :

- ✅ Module Alertes : Création, workflow, résolution
- ✅ Configuration OTA : Assistant provisioning + push MQTT
- ✅ Gestion certificats TLS : Upload, suivi expirations
- ✅ Notifications : Email + WebSocket push
- ✅ Rapports : Export CSV/PDF inventaire + alertes
- ✅ Audit trail : Traçabilité actions utilisateurs

**Planning détaillé** :

| Semaine | Tâches | Responsable |
|---------|--------|-------------|
| S9 | API Alerts (CRUD) + workflow statuts | Dev Backend |
| S10 | Frontend module Alertes + notifications | Dev Frontend |
| S11 | Assistant provisioning (wizard 5 étapes) | Dev Frontend |
| S12 | Configuration OTA (MQTT publish) | Dev Backend |
| S13 | Gestion certificats (upload, parsing X.509) | Dev Backend |
| S14 | Rapports exports (CSV, PDF) | Dev Backend + Frontend |
| S15 | Tests e2e, debug | Full stack |
| S16 | Déploiement production, documentation | DevOps |

**Budget Phase 2** : 40 jours × 2 devs = 80 j·h (~24k€)

### 9.3 Phase 3 : Optimisations & Intégrations (1 mois)

**Objectif** : Performance, scalabilité, intégrations tierces

**Livrables** :

- ✅ Optimisation base données (indexation, partitionnement)
- ✅ Cache Redis (sessions, rate limiting, cache API)
- ✅ Webhooks configurables
- ✅ API Grafana/Prometheus
- ✅ Tests charge (100+ users simultanés)
- ✅ Documentation complète (technique + utilisateur)

**Budget Phase 3** : 20 jours × 2 devs = 40 j·h (~12k€)

### 9.4 Roadmap Total

```
┌─────────────────────────────────────────────────────────────────┐
│ Phase 1 : MVP (2 mois)                                          │
│ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
│                                                                 │
│ Phase 2 : Avancées (2 mois)                                    │
│ ░░░░░░░░░░░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░│
│                                                                 │
│ Phase 3 : Optimisations (1 mois)                               │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░│
│                                                                 │
│ Timeline: Févr 2026 ──────────► Juillet 2026                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 10. Budget et Ressources

### 10.1 Équipe Projet

| Rôle | Profil | Charge | Coût/jour | Total |
|------|--------|--------|-----------|-------|
| **Dev Frontend** | React + TypeScript, UX | 100 j | 350€ | 35k€ |
| **Dev Backend** | Node.js + PostgreSQL, MQTT | 100 j | 400€ | 40k€ |
| **DevOps** | Docker, CI/CD, monitoring | 20 j | 450€ | 9k€ |
| **Product Owner** | Specs, recette, documentation | 30 j | 300€ | 9k€ |
| **UI/UX Designer** | Maquettes, wireframes | 10 j | 350€ | 3.5k€ |

**Total ressources humaines** : **96.5k€**

### 10.2 Infrastructure

| Poste | Provider | Coût/mois | Coût annuel |
|-------|----------|-----------|-------------|
| Serveur web (VPS) | OVH, Hetzner | 40€ | 480€ |
| Base données PostgreSQL | Cloud SQL (GCP) | 80€ | 960€ |
| Redis cache | Managed Redis | 30€ | 360€ |
| Stockage fichiers (S3) | AWS S3 | 20€ | 240€ |
| Certificat SSL | Let's Encrypt | 0€ | 0€ |
| Monitoring (Grafana Cloud) | Grafana Labs | 25€ | 300€ |
| Backup (automatisé) | Restic + S3 | 15€ | 180€ |

**Total infrastructure annuel** : **2.52k€**

### 10.3 Budget Total

| Phase | Durée | Coût |
|-------|-------|------|
| Phase 1 (MVP) | 2 mois | 35k€ |
| Phase 2 (Avancées) | 2 mois | 35k€ |
| Phase 3 (Optimisations) | 1 mois | 18k€ |
| Infrastructure (1ère année) | 12 mois | 2.5k€ |
| Contingence (10%) | - | 9k€ |

**TOTAL PROJET** : **99.5k€** (~100k€)

### 10.4 ROI (Retour sur Investissement)

**Gains attendus** :

| Gain | Avant | Après | Économie annuelle |
|------|-------|-------|-------------------|
| Temps provisioning nœud | 2h | 15min | 60 j·h × 300€ = **18k€** |
| Temps diagnostic incident | 4h | 30min | 80 j·h × 300€ = **24k€** |
| Downtime évité (monitoring) | 10h/an | 1h/an | 9h × 10k€/h = **90k€** |
| Conformité NIS2 (amendes évitées) | Risque 10M€ | Conforme | **Inestimable** |

**ROI estimé** : **< 1 an** (132k€ gains vs 100k€ investissement)

---

## 11. Livrables

### 11.1 Livrables Techniques

| Livrable | Format | Responsable |
|----------|--------|-------------|
| **Code source** | GitHub repository | Dev Frontend + Backend |
| **Base données** | PostgreSQL dump + migrations | Dev Backend |
| **Documentation API** | Swagger JSON/YAML | Dev Backend |
| **Tests automatisés** | Jest (backend), Cypress (frontend) | Dev Full stack |
| **Docker Compose** | docker-compose.yml | DevOps |
| **Scripts déploiement** | Bash, Ansible | DevOps |

### 11.2 Livrables Fonctionnels

| Livrable | Format | Responsable |
|----------|--------|-------------|
| **Manuel utilisateur** | PDF, HTML | Product Owner |
| **Guide administrateur** | Markdown, PDF | DevOps |
| **Vidéos tutoriels** | MP4 (5-10min) | Product Owner |
| **Procédures maintenance** | Markdown | DevOps |
| **Rapport conformité NIS2** | PDF | Product Owner + RSSI |

### 11.3 Formation

**Formations prévues** :

1. **Utilisateurs finaux** (2h) :
   - Découverte interface
   - Gestion quotidienne (dashboard, alertes)
   - Bonnes pratiques

2. **Administrateurs système** (4h) :
   - Configuration avancée
   - Provisioning nœuds
   - Gestion certificats
   - Troubleshooting

3. **Support technique** (6h) :
   - Architecture complète
   - Base de données
   - API REST
   - Déploiement et monitoring

---

## 12. Risques et Mitigation

### 12.1 Risques Identifiés

| Risque | Probabilité | Impact | Mitigation |
|--------|-------------|--------|------------|
| **Dépassement délai** | Moyenne | Élevé | Planning réaliste + buffer 10%, revues hebdo |
| **Scalabilité insuffisante** | Faible | Élevé | Tests charge dès MVP, architecture modulaire |
| **Sécurité (vulnérabilités)** | Moyenne | Critique | Audit sécurité externe, SAST/DAST continu |
| **Intégration MQTT complexe** | Moyenne | Moyen | POC semaine 1, tests intensifs |
| **Turnover équipe** | Faible | Élevé | Documentation continue, code review |

### 12.2 Plan de Contingence

**Scénario 1 : Dépassement budget 20%**
- Solution : Réduire scope Phase 3, reporter intégrations tierces

**Scénario 2 : Indisponibilité dev clé (maladie)**
- Solution : Pair programming, documentation code, backup dev externe

**Scénario 3 : Problème performance (latence WebSocket)**
- Solution : Migration Redis Pub/Sub, optimisation queries SQL

---

## 13. Validation et Acceptation

### 13.1 Critères d'Acceptation MVP

| Critère | Validation |
|---------|------------|
| ✅ Dashboard affiche 150 dispositifs < 2s | Test charge |
| ✅ WebSocket latency < 500ms (95e percentile) | Monitoring Prometheus |
| ✅ Authentification JWT + RBAC fonctionnel | Tests automatisés |
| ✅ MQTT integration : données capteurs reçues temps réel | Tests manuels |
| ✅ Interface responsive (mobile + desktop) | Tests navigateurs |

### 13.2 Recette Utilisateur

**Processus** :

1. **Recette fonctionnelle** (5 jours) :
   - Scénarios utilisateur (20+ cas)
   - Validation Product Owner
   - Correction bugs bloquants

2. **Recette technique** (3 jours) :
   - Tests charge (100 users simultanés)
   - Tests sécurité (OWASP Top 10)
   - Validation DevOps

3. **Mise en production** (1 jour) :
   - Déploiement staging → production
   - Formation utilisateurs finaux
   - Support post-déploiement (2 semaines)

---

## 14. Support et Maintenance

### 14.1 Garantie

**Période** : 3 mois après mise en production

**Inclus** :
- ✅ Correction bugs (réponse < 48h)
- ✅ Support utilisateurs (email, ticketing)
- ✅ Hotfix critiques (déploiement < 4h)

### 14.2 Maintenance Continue (TMA)

**Forfait mensuel** : 5k€/mois

**Inclus** :
- ✅ Monitoring 24/7 (alertes critiques)
- ✅ Mises à jour sécurité (OS, dépendances)
- ✅ Évolutions mineures (10h/mois)
- ✅ Backup vérification hebdomadaire
- ✅ Rapports mensuels (KPI, incidents)

---

## 15. Annexes

### 15.1 Glossaire

| Terme | Définition |
|-------|------------|
| **CMDB** | Configuration Management Database - Inventaire dispositifs |
| **JWT** | JSON Web Token - Mécanisme authentification |
| **MQTT** | Message Queuing Telemetry Transport - Protocole IoT |
| **NIS2** | Network and Information Systems Directive 2 - Directive UE cybersécurité |
| **OTA** | Over-The-Air - Mise à jour à distance |
| **RBAC** | Role-Based Access Control - Contrôle accès par rôles |
| **WebSocket** | Protocole communication bidirectionnelle temps réel |

### 15.2 Références

| Document | Lien |
|----------|------|
| **Audit NIS2 APRU40** | [AUDIT_SECURITE_NIS2.md](AUDIT_SECURITE_NIS2.md) |
| **Architecture IoT** | [ARCHITECTURE.md](ARCHITECTURE.md) |
| **Guide déploiement sécurisé** | [DEPLOYMENT_GUIDE_SECURED.md](DEPLOYMENT_GUIDE_SECURED.md) |
| **Directive NIS2** | https://eur-lex.europa.eu/eli/dir/2022/2555 |

---

**FIN DU CAHIER DES CHARGES**

**Approbation** :
- [ ] Product Owner : __________________ Date : __________
- [ ] RSSI : __________________ Date : __________
- [ ] Direction Technique : __________________ Date : __________

**Prochaine révision** : 1er mars 2026
