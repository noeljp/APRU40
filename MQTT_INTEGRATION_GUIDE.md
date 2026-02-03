# Guide d'intégration MQTT - Passerelle APRU40

## Vue d'ensemble

Ce guide explique comment configurer et utiliser le composant `mqtt_manager` pour connecter les passerelles APRU40 à un broker Mosquitto avec TLS 1.3.

## Architecture

```
┌─────────────────┐         ESP-NOW (AES+HMAC)         ┌──────────────┐
│  Nœud 1 (ESP32) │────────────────────────────────────▶│              │
└─────────────────┘                                      │              │
                                                         │  Passerelle  │
┌─────────────────┐         ESP-NOW (AES+HMAC)         │   (ESP32)    │
│  Nœud 2 (ESP32) │────────────────────────────────────▶│              │
└─────────────────┘                                      │  + Ethernet  │
                                                         │              │
       ...                                               └──────┬───────┘
                                                                │
┌─────────────────┐         ESP-NOW (AES+HMAC)                │
│  Nœud N (ESP32) │────────────────────────────────────────────┘
└─────────────────┘                                             │
                                                                │
                                                 MQTT/TLS 1.3   │
                                                (Ethernet PoE)  │
                                                                │
                                                                ▼
                                                    ┌───────────────────┐
                                                    │  Broker Mosquitto │
                                                    │   (Cloud/Local)   │
                                                    └─────────┬─────────┘
                                                              │
                                                              ▼
                                                    ┌───────────────────┐
                                                    │  Serveur Backend  │
                                                    │  (API, Database)  │
                                                    └───────────────────┘
```

## Configuration du broker Mosquitto

### 1. Installation

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install mosquitto mosquitto-clients

# Docker
docker run -d -p 8883:8883 -p 9001:9001 \
  --name mosquitto \
  -v /path/to/config:/mosquitto/config \
  -v /path/to/certs:/mosquitto/certs \
  eclipse-mosquitto
```

### 2. Génération des certificats

```bash
# Créer répertoire pour certificats
mkdir -p mosquitto/certs
cd mosquitto/certs

# 1. Générer CA (Certificate Authority)
openssl req -new -x509 -days 3650 -extensions v3_ca \
  -keyout ca.key -out ca.crt \
  -subj "/C=FR/ST=IDF/L=Paris/O=APRU40/CN=APRU40-CA"

# 2. Générer clé serveur
openssl genrsa -out server.key 2048

# 3. Générer CSR serveur
openssl req -new -key server.key -out server.csr \
  -subj "/C=FR/ST=IDF/L=Paris/O=APRU40/CN=broker.apru40.local"

# 4. Signer certificat serveur avec CA
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out server.crt -days 3650

# 5. Générer clé client (gateway)
openssl genrsa -out client.key 2048

# 6. Générer CSR client
openssl req -new -key client.key -out client.csr \
  -subj "/C=FR/ST=IDF/L=Paris/O=APRU40/CN=APRU40_GW001"

# 7. Signer certificat client avec CA
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out client.crt -days 3650

# Permissions
chmod 600 *.key
chmod 644 *.crt
```

### 3. Configuration Mosquitto (`mosquitto.conf`)

```conf
# Listener MQTT over TLS
listener 8883
protocol mqtt

# Certificats serveur
cafile /mosquitto/certs/ca.crt
certfile /mosquitto/certs/server.crt
keyfile /mosquitto/certs/server.key

# TLS 1.3 uniquement
tls_version tlsv1.3

# Authentification mutuelle (mTLS)
require_certificate true
use_identity_as_username true

# Paramètres de sécurité
allow_anonymous false

# Authentification par mot de passe (optionnel si mTLS)
password_file /mosquitto/config/passwd

# Logging
log_dest file /mosquitto/log/mosquitto.log
log_type all
log_timestamp true
log_timestamp_format %Y-%m-%d %H:%M:%S

# Persistance
persistence true
persistence_location /mosquitto/data/

# Keepalive
max_keepalive 60

# ACL (Access Control List)
acl_file /mosquitto/config/acl.conf
```

### 4. Fichier ACL (`acl.conf`)

```conf
# Permissions par défaut
user anonymous
topic read $SYS/#

# Gateway APRU40_GW001
user APRU40_GW001
topic readwrite apru40/gateway/APRU40_GW001/#
topic write apru40/node/+/config
topic write apru40/node/+/ota
topic read apru40/ota/#

# Gateway APRU40_GW002
user APRU40_GW002
topic readwrite apru40/gateway/APRU40_GW002/#
topic write apru40/node/+/config
topic write apru40/node/+/ota
topic read apru40/ota/#

# Backend server
user backend_server
topic read apru40/gateway/+/data
topic read apru40/gateway/+/status
topic write apru40/gateway/+/cmd
topic write apru40/gateway/+/config
topic write apru40/node/+/config
topic write apru40/ota/#
```

### 5. Démarrage

```bash
# Démarrer Mosquitto
mosquitto -c /path/to/mosquitto.conf -v

# Ou avec systemd
sudo systemctl start mosquitto
sudo systemctl enable mosquitto
```

## Configuration ESP32 (Passerelle)

### 1. Préparation des certificats

Copier les certificats dans le système de fichiers SPIFFS de l'ESP32 :

```
spiffs/
├── ca.crt          # Certificat CA (public)
├── client.crt      # Certificat client gateway (public)
└── client.key      # Clé privée client gateway (privé !)
```

Outils pour flasher SPIFFS :
```bash
# PlatformIO
pio run --target uploadfs

# ESP-IDF
python $IDF_PATH/components/spiffs/spiffsgen.py 0x1F0000 spiffs spiffs.bin
esptool.py --port /dev/ttyUSB0 write_flash 0x610000 spiffs.bin
```

### 2. Configuration dans `node_config.h`

```c
#define NODE_MODE MODE_GATEWAY

#define ENABLE_ETHERNET 1
#define ENABLE_BLUETOOTH_SPP 0  // Pas de BT sur gateway

// Broker MQTT
#define MQTT_BROKER_URI "mqtts://broker.apru40.local:8883"
#define MQTT_CLIENT_ID "APRU40_GW001"

// Certificats
#define MQTT_CA_CERT_PATH "/spiffs/ca.crt"
#define MQTT_CLIENT_CERT_PATH "/spiffs/client.crt"
#define MQTT_CLIENT_KEY_PATH "/spiffs/client.key"
```

### 3. Initialisation dans `app_main()`

```c
#if NODE_MODE == MODE_GATEWAY && ENABLE_ETHERNET

// Initialiser Ethernet
esp_netif_init();
esp_event_loop_create_default();
esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
esp_netif_t *eth_netif = esp_netif_new(&cfg);

// Configuration PHY Ethernet (ESP32-POE-ISO = LAN8720)
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
phy_config.phy_addr = ETH_PHY_ADDR;
phy_config.reset_gpio_num = -1;

esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
esp_eth_handle_t eth_handle = NULL;
ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));
ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
ESP_ERROR_CHECK(esp_eth_start(eth_handle));

ESP_LOGI(TAG, "Ethernet initialisé, attente connexion...");

// Attendre IP (avec timeout)
// ... code attente DHCP ...

// Charger certificats depuis SPIFFS
char *ca_cert = NULL, *client_cert = NULL, *client_key = NULL;
// TODO: Implémenter lecture depuis SPIFFS
// Pour dev, utiliser certificats en dur (extern const char ca_cert_pem[])

// Initialiser MQTT Manager
mqtt_manager_config_t mqtt_config = {
    .broker_uri = MQTT_BROKER_URI,
    .client_id = MQTT_CLIENT_ID,
    .username = NULL,  // mTLS, pas besoin de username/password
    .password = NULL,
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

// S'abonner aux topics de commandes
mqtt_manager_subscribe(MQTT_TOPIC_CMD, 1);
mqtt_manager_subscribe(MQTT_TOPIC_CONFIG, 1);
mqtt_manager_subscribe(MQTT_TOPIC_NODE_CONFIG, 1);

ESP_LOGI(TAG, "MQTT connecté et souscriptions actives");

#endif // MODE_GATEWAY
```

### 4. Callbacks MQTT

```c
#if NODE_MODE == MODE_GATEWAY

/**
 * @brief Callback réception messages MQTT
 */
static void mqtt_message_callback(const char *topic, const char *data, size_t len)
{
    ESP_LOGI(TAG, "📨 MQTT RX: %s (%zu octets)", topic, len);
    
    // Commandes gateway
    if (strstr(topic, "/cmd")) {
        // Parser commande JSON
        // Exemples : {"cmd":"restart"}, {"cmd":"get_status"}
        ESP_LOGI(TAG, "  Commande: %.*s", len, data);
        // TODO: Traiter commandes
    }
    
    // Configuration gateway
    else if (strstr(topic, "gateway") && strstr(topic, "/config")) {
        // Mise à jour config gateway
        ESP_LOGI(TAG, "  Config gateway: %.*s", len, data);
        // TODO: Parser et appliquer config
    }
    
    // Configuration nœud
    else if (strstr(topic, "node") && strstr(topic, "/config")) {
        // Extraire node_id du topic
        int node_id = 0;
        sscanf(topic, "apru40/node/%d/config", &node_id);
        
        ESP_LOGI(TAG, "  Config pour nœud %d: %.*s", node_id, len, data);
        
        // Transférer config au nœud via ESP-NOW
        // esp_now_secure_send_to_node(node_id, data, len);
    }
}

/**
 * @brief Callback événements connexion MQTT
 */
static void mqtt_connection_callback(bool connected)
{
    if (connected) {
        ESP_LOGI(TAG, "✅ MQTT connecté au broker");
        
        // Publier heartbeat initial
        mqtt_manager_publish_heartbeat();
    } else {
        ESP_LOGW(TAG, "❌ MQTT déconnecté, reconnexion...");
    }
}

#endif // MODE_GATEWAY
```

## Topics MQTT

### Publication (Gateway → Cloud)

| Topic | QoS | Retain | Description |
|-------|-----|--------|-------------|
| `apru40/gateway/{gw_id}/data` | 1 | false | Données agrégées des nœuds |
| `apru40/gateway/{gw_id}/status` | 1 | true | Heartbeat gateway (online/offline) |

**Format `data`** (JSON) :
```json
{
  "gateway_id": "APRU40_GW001",
  "timestamp": 1738454400,
  "nodes": [
    {
      "node_id": 1,
      "qr_code": "PROD12345",
      "rssi": -45,
      "sensors": {
        "ads7128": {
          "ch0": {"raw": 2048, "value": 12.5, "unit": "mA"},
          "ch1": {"raw": 1500, "value": 45.3, "unit": "°C"}
        }
      }
    }
  ]
}
```

**Format `status`** (JSON) :
```json
{
  "status": "online",
  "timestamp": 1738454400,
  "uptime": 86400,
  "free_heap": 150000,
  "nodes_connected": 5
}
```

### Souscription (Cloud → Gateway)

| Topic | QoS | Description |
|-------|-----|-------------|
| `apru40/gateway/{gw_id}/cmd` | 1 | Commandes gateway |
| `apru40/gateway/{gw_id}/config` | 1 | Configuration gateway |
| `apru40/node/+/config` | 1 | Configuration nœuds (wildcard) |
| `apru40/node/+/ota` | 1 | Firmware OTA nœuds |

**Format `cmd`** (JSON) :
```json
{"cmd": "restart"}
{"cmd": "get_status"}
{"cmd": "get_nodes"}
```

**Format `config`** (JSON) :
```json
{
  "node_id": 1,
  "config": {
    "acquisition": {
      "ads7128_period_ms": 2000
    },
    "conversions": {
      "ads7128_ch0": {
        "type": "linear",
        "a": 0.00488,
        "b": 4.0
      }
    }
  }
}
```

## Tests

### Test connexion TLS

```bash
# Vérifier certificat serveur
openssl s_client -connect broker.apru40.local:8883 -CAfile ca.crt

# Test avec client mTLS
mosquitto_pub -h broker.apru40.local -p 8883 \
  --cafile ca.crt --cert client.crt --key client.key \
  -t "apru40/gateway/APRU40_GW001/status" \
  -m '{"status":"online"}' \
  -q 1
```

### Test souscription

```bash
# Écouter toutes les données
mosquitto_sub -h broker.apru40.local -p 8883 \
  --cafile ca.crt --cert client.crt --key client.key \
  -t "apru40/gateway/+/data" \
  -q 1 -v
```

### Test publication commande

```bash
# Envoyer commande à gateway
mosquitto_pub -h broker.apru40.local -p 8883 \
  --cafile ca.crt --cert backend.crt --key backend.key \
  -t "apru40/gateway/APRU40_GW001/cmd" \
  -m '{"cmd":"get_status"}' \
  -q 1
```

## Dépannage

### Gateway ne se connecte pas

1. Vérifier connectivité Ethernet :
```bash
# Sur ESP32 (logs série)
[ETH] Link Up
[ETH] IP: 192.168.1.100
```

2. Vérifier résolution DNS :
```bash
# Sur PC
ping broker.apru40.local
```

3. Vérifier certificats :
```bash
# Valider certificat client
openssl verify -CAfile ca.crt client.crt
```

4. Logs MQTT ESP32 :
```
[MQTT_MGR] MQTT manager initialisé (broker: mqtts://...)
[MQTT_MGR] Client MQTT démarré
[MQTT_MGR] ✅ Connecté au broker MQTT
```

### Certificats expirés

```bash
# Vérifier dates validité
openssl x509 -in client.crt -noout -dates

# Régénérer si expiré
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key \
  -CAcreateserial -out client.crt -days 3650
```

### Permissions MQTT refusées

Vérifier ACL dans `mosquitto.conf` et que le CN du certificat correspond au username.

```bash
# CN certificat client
openssl x509 -in client.crt -noout -subject
# Doit correspondre à "user" dans acl.conf
```

## Sécurité Production

1. **Certificats** :
   - CA privée sécurisée (offline)
   - Rotation certificats tous les 1-2 ans
   - Révocation certificats compromis (CRL)

2. **Réseau** :
   - Firewall : Port 8883 uniquement pour IPs gateways
   - VLAN dédié pour gateways
   - VPN si broker distant

3. **Broker** :
   - Rate limiting
   - Monitoring connexions suspectes
   - Logs d'audit
   - Sauvegarde configuration

4. **ESP32** :
   - Secure Boot activé
   - Flash Encryption
   - Certificats en NVS chiffré (pas SPIFFS)

---

**Prochaines étapes** :
- Implémenter `mqtt_manager_load_certs()` pour SPIFFS
- Ajouter support OTA firmware via MQTT
- Implémenter parser JSON pour configurations
- Ajouter métriques Prometheus/Grafana
