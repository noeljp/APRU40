#include "esp_now_secure.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include <string.h>

#define TAG "ESP_NOW_SEC"
#define MAX_TRUSTED_PEERS 32

/* Structure du paquet sécurisé */
typedef struct __attribute__((packed)) {
    uint8_t node_id;           // ID du nœud émetteur
    uint32_t counter;          // Compteur anti-replay
    uint8_t iv[16];            // Vecteur d'initialisation AES
    uint8_t data[ESP_NOW_SECURE_MAX_DATA_LEN];  // Données chiffrées
    uint8_t hmac[32];          // HMAC-SHA256 pour authentification
} secure_packet_t;

/* Variables globales */
static esp_now_secure_config_t g_config = {0};
static uint32_t g_tx_counter = 0;
static uint32_t g_last_rx_counter[256] = {0};  // Par node_id
static uint8_t g_trusted_peers[MAX_TRUSTED_PEERS][6] = {0};
static uint8_t g_trusted_peers_count = 0;
static bool g_use_whitelist = false;

/* Statistiques */
static uint32_t g_stats_valid = 0;
static uint32_t g_stats_invalid_hmac = 0;
static uint32_t g_stats_replay = 0;
static uint32_t g_stats_untrusted = 0;

/* Forward declarations */
static void esp_now_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len);
static void esp_now_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);
static bool verify_hmac(const uint8_t *packet, size_t packet_len, const uint8_t *expected_hmac);
static void compute_hmac(const uint8_t *data, size_t len, uint8_t *hmac_out);
static bool is_trusted_peer(const uint8_t *mac_addr);

esp_err_t esp_now_secure_init(const esp_now_secure_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Configuration NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copier configuration
    memcpy(&g_config, config, sizeof(esp_now_secure_config_t));
    
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Init WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Configurer canal si spécifié
    if (config->channel > 0 && config->channel <= 13) {
        ESP_ERROR_CHECK(esp_wifi_set_channel(config->channel, WIFI_SECOND_CHAN_NONE));
        ESP_LOGI(TAG, "Canal WiFi configuré: %d", config->channel);
    }
    
    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(esp_now_send_cb));
    
    // Ajouter peer broadcast
    esp_now_peer_info_t peer = {0};
    memset(peer.peer_addr, 0xFF, 6);  // Broadcast
    peer.channel = 0;
    peer.encrypt = false;  // Pas de chiffrement ESP-NOW natif (on fait le notre)
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    
    // Initialiser compteur TX (random pour éviter collisions)
    g_tx_counter = esp_random();
    
    ESP_LOGI(TAG, "ESP-NOW sécurisé initialisé - Node ID: %d", config->node_id);
    
    return ESP_OK;
}

esp_err_t esp_now_secure_send(const uint8_t *data, uint8_t len) {
    if (!data || len == 0 || len > ESP_NOW_SECURE_MAX_DATA_LEN) {
        ESP_LOGE(TAG, "Données invalides (len=%d)", len);
        return ESP_ERR_INVALID_ARG;
    }
    
    secure_packet_t packet = {0};
    
    // Header
    packet.node_id = g_config.node_id;
    packet.counter = g_tx_counter++;
    
    // Générer IV aléatoire pour AES-CBC
    esp_fill_random(packet.iv, 16);
    
    // Chiffrer les données avec AES-256-CBC
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, g_config.aes_key, 256);
    
    // Padding PKCS#7
    uint8_t padded_data[ESP_NOW_SECURE_MAX_DATA_LEN];
    memcpy(padded_data, data, len);
    uint8_t padding = 16 - (len % 16);
    if (padding == 16) padding = 0;
    for (int i = 0; i < padding; i++) {
        padded_data[len + i] = padding;
    }
    size_t padded_len = len + padding;
    
    // Chiffrer par blocs de 16 octets
    uint8_t iv_copy[16];
    memcpy(iv_copy, packet.iv, 16);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, iv_copy, padded_data, packet.data);
    mbedtls_aes_free(&aes);
    
    // Calculer HMAC sur tout le paquet (sauf le HMAC lui-même)
    size_t hmac_input_len = sizeof(packet.node_id) + sizeof(packet.counter) + 
                           sizeof(packet.iv) + padded_len;
    compute_hmac((uint8_t*)&packet, hmac_input_len, packet.hmac);
    
    // Envoyer via ESP-NOW
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_err_t err = esp_now_send(broadcast_mac, (uint8_t*)&packet, sizeof(packet));
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erreur envoi ESP-NOW: %d", err);
    }
    
    return err;
}

static void esp_now_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(secure_packet_t)) {
        ESP_LOGW(TAG, "Paquet taille incorrecte: %d (attendu %d)", len, sizeof(secure_packet_t));
        return;
    }
    
    secure_packet_t *packet = (secure_packet_t*)data;
    
    // Vérifier whitelist si activée
    if (g_use_whitelist && !is_trusted_peer(info->src_addr)) {
        g_stats_untrusted++;
        ESP_LOGW(TAG, "Paquet de peer non autorisé: %02X:%02X:%02X:%02X:%02X:%02X",
                 info->src_addr[0], info->src_addr[1], info->src_addr[2],
                 info->src_addr[3], info->src_addr[4], info->src_addr[5]);
        return;
    }
    
    // Vérifier HMAC
    size_t hmac_input_len = len - sizeof(packet->hmac);
    if (!verify_hmac((uint8_t*)packet, hmac_input_len, packet->hmac)) {
        g_stats_invalid_hmac++;
        ESP_LOGW(TAG, "HMAC invalide de node %d", packet->node_id);
        return;
    }
    
    // Vérifier counter anti-replay
    if (packet->counter <= g_last_rx_counter[packet->node_id]) {
        g_stats_replay++;
        ESP_LOGW(TAG, "Replay attack détecté: node %d, counter %lu <= %lu",
                 packet->node_id, packet->counter, g_last_rx_counter[packet->node_id]);
        return;
    }
    g_last_rx_counter[packet->node_id] = packet->counter;
    
    // Déchiffrer les données
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, g_config.aes_key, 256);
    
    uint8_t decrypted[ESP_NOW_SECURE_MAX_DATA_LEN];
    uint8_t iv_copy[16];
    memcpy(iv_copy, packet->iv, 16);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ESP_NOW_SECURE_MAX_DATA_LEN, 
                          iv_copy, packet->data, decrypted);
    mbedtls_aes_free(&aes);
    
    // Retirer padding PKCS#7
    uint8_t padding = decrypted[ESP_NOW_SECURE_MAX_DATA_LEN - 1];
    if (padding > 16) padding = 0;
    uint8_t decrypted_len = ESP_NOW_SECURE_MAX_DATA_LEN - padding;
    
    g_stats_valid++;
    
    // Appeler callback utilisateur
    if (g_config.recv_cb) {
        g_config.recv_cb(info->src_addr, decrypted, decrypted_len, info->rx_ctrl->rssi);
    }
}

static void esp_now_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (g_config.send_cb) {
        // ESP-IDF 5.5.0 : wifi_tx_info_t ne contient pas l'adresse MAC du destinataire
        // On passe NULL ou l'adresse broadcast (0xFF:FF:FF:FF:FF:FF) pour broadcast
        static const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        g_config.send_cb(broadcast_mac, status);
    }
}

static void compute_hmac(const uint8_t *data, size_t len, uint8_t *hmac_out) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, g_config.hmac_key, 32);
    mbedtls_md_hmac_update(&ctx, data, len);
    mbedtls_md_hmac_finish(&ctx, hmac_out);
    mbedtls_md_free(&ctx);
}

static bool verify_hmac(const uint8_t *packet, size_t packet_len, const uint8_t *expected_hmac) {
    uint8_t computed_hmac[32];
    compute_hmac(packet, packet_len, computed_hmac);
    return memcmp(computed_hmac, expected_hmac, 32) == 0;
}

esp_err_t esp_now_secure_add_trusted_peer(const uint8_t *mac_addr) {
    if (!mac_addr) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_trusted_peers_count >= MAX_TRUSTED_PEERS) {
        ESP_LOGE(TAG, "Whitelist pleine (%d peers max)", MAX_TRUSTED_PEERS);
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(g_trusted_peers[g_trusted_peers_count], mac_addr, 6);
    g_trusted_peers_count++;
    g_use_whitelist = true;
    
    ESP_LOGI(TAG, "Peer ajouté à la whitelist: %02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    
    return ESP_OK;
}

static bool is_trusted_peer(const uint8_t *mac_addr) {
    for (int i = 0; i < g_trusted_peers_count; i++) {
        if (memcmp(g_trusted_peers[i], mac_addr, 6) == 0) {
            return true;
        }
    }
    return false;
}

esp_err_t esp_now_secure_get_local_mac(uint8_t *mac_addr) {
    if (!mac_addr) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_wifi_get_mac(WIFI_IF_STA, mac_addr);
}

void esp_now_secure_get_stats(uint32_t *valid_packets, uint32_t *invalid_hmac, 
                               uint32_t *replay_attacks, uint32_t *untrusted_peers) {
    if (valid_packets) *valid_packets = g_stats_valid;
    if (invalid_hmac) *invalid_hmac = g_stats_invalid_hmac;
    if (replay_attacks) *replay_attacks = g_stats_replay;
    if (untrusted_peers) *untrusted_peers = g_stats_untrusted;
}

void esp_now_secure_deinit(void) {
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    ESP_LOGI(TAG, "ESP-NOW sécurisé désinitialisé");
}
