/**
 * @file mqtt_manager.c
 * @brief Implémentation du gestionnaire MQTT pour passerelles APRU40
 */

#include "mqtt_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#define TAG "MQTT_MGR"

// Namespace NVS pour certificats (chiffré si CONFIG_NVS_ENCRYPTION=y)
#define NVS_NAMESPACE_CERTS "mqtt_certs"

/* ============================================================================
 * VARIABLES PRIVÉES
 * ============================================================================ */

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static mqtt_message_callback_t s_msg_callback = NULL;
static mqtt_connection_callback_t s_conn_callback = NULL;
static mqtt_manager_stats_t s_stats = {0};
static SemaphoreHandle_t s_stats_mutex = NULL;
static char s_client_id[32] = {0};

/* ============================================================================
 * FONCTIONS PRIVÉES
 * ============================================================================ */

/**
 * @brief Handler événements MQTT
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ Connecté au broker MQTT");
            
            if (s_stats_mutex) {
                xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
                s_stats.is_connected = true;
                s_stats.reconnections++;
                xSemaphoreGive(s_stats_mutex);
            }
            
            if (s_conn_callback) {
                s_conn_callback(true);
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "❌ Déconnecté du broker MQTT");
            
            if (s_stats_mutex) {
                xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
                s_stats.is_connected = false;
                xSemaphoreGive(s_stats_mutex);
            }
            
            if (s_conn_callback) {
                s_conn_callback(false);
            }
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Souscription OK (msg_id=%d)", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "Désouscription OK (msg_id=%d)", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "Publication OK (msg_id=%d)", event->msg_id);
            
            if (s_stats_mutex) {
                xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
                s_stats.messages_published++;
                xSemaphoreGive(s_stats_mutex);
            }
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "📨 Message reçu: %.*s", event->topic_len, event->topic);
            
            if (s_stats_mutex) {
                xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
                s_stats.messages_received++;
                xSemaphoreGive(s_stats_mutex);
            }
            
            if (s_msg_callback && event->topic && event->data) {
                // Créer copies null-terminated
                char *topic = malloc(event->topic_len + 1);
                char *data = malloc(event->data_len + 1);
                
                if (topic && data) {
                    memcpy(topic, event->topic, event->topic_len);
                    topic[event->topic_len] = '\0';
                    
                    memcpy(data, event->data, event->data_len);
                    data[event->data_len] = '\0';
                    
                    s_msg_callback(topic, data, event->data_len);
                    
                    free(topic);
                    free(data);
                }
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erreur MQTT");
            
            if (s_stats_mutex) {
                xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
                s_stats.last_error = -1;
                xSemaphoreGive(s_stats_mutex);
            }
            
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "  Erreur transport TCP");
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "  Connexion refusée");
            }
            break;
            
        default:
            ESP_LOGD(TAG, "Événement MQTT autre: %d", event_id);
            break;
    }
}

/* ============================================================================
 * API PUBLIQUE
 * ============================================================================ */

esp_err_t mqtt_manager_init(const mqtt_manager_config_t *config)
{
    if (!config || !config->broker_uri) {
        ESP_LOGE(TAG, "Configuration invalide");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_mqtt_client != NULL) {
        ESP_LOGW(TAG, "MQTT déjà initialisé");
        return ESP_OK;
    }
    
    // Créer mutex pour stats
    s_stats_mutex = xSemaphoreCreateMutex();
    if (!s_stats_mutex) {
        ESP_LOGE(TAG, "Échec création mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Sauvegarder callbacks
    s_msg_callback = config->msg_cb;
    s_conn_callback = config->conn_cb;
    
    // Sauvegarder client_id
    if (config->client_id) {
        strncpy(s_client_id, config->client_id, sizeof(s_client_id) - 1);
    }
    
    // Configuration MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = config->broker_uri,
        .broker.verification.certificate = config->ca_cert_pem,
        .credentials = {
            .authentication = {
                .certificate = config->client_cert_pem,
                .key = config->client_key_pem,
            },
            .username = config->username,
            .client_id = config->client_id,
        },
        .session = {
            .keepalive = config->keepalive_sec > 0 ? config->keepalive_sec : 60,
            .disable_clean_session = false,
        },
    };
    
    // Créer client MQTT
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "Échec init client MQTT");
        vSemaphoreDelete(s_stats_mutex);
        s_stats_mutex = NULL;
        return ESP_FAIL;
    }
    
    // Enregistrer handler événements
    esp_err_t err = esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, 
                                                    mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec enregistrement handler: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        vSemaphoreDelete(s_stats_mutex);
        s_stats_mutex = NULL;
        return err;
    }
    
    // Initialiser stats
    memset(&s_stats, 0, sizeof(s_stats));
    
    ESP_LOGI(TAG, "MQTT manager initialisé (broker: %s)", config->broker_uri);
    return ESP_OK;
}

esp_err_t mqtt_manager_start(void)
{
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "MQTT non initialisé");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec démarrage client: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "Client MQTT démarré");
    return ESP_OK;
}

esp_err_t mqtt_manager_stop(void)
{
    if (!s_mqtt_client) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = esp_mqtt_client_stop(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Échec arrêt client: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "Client MQTT arrêté");
    return ESP_OK;
}

esp_err_t mqtt_manager_subscribe(const char *topic, uint8_t qos)
{
    if (!s_mqtt_client || !topic) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_stats.is_connected) {
        ESP_LOGW(TAG, "Pas connecté, souscription différée");
    }
    
    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Échec souscription topic %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Souscription topic: %s (QoS %d)", topic, qos);
    return ESP_OK;
}

esp_err_t mqtt_manager_unsubscribe(const char *topic)
{
    if (!s_mqtt_client || !topic) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int msg_id = esp_mqtt_client_unsubscribe(s_mqtt_client, topic);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Échec désouscription topic %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Désouscription topic: %s", topic);
    return ESP_OK;
}

esp_err_t mqtt_manager_publish(const char *topic, const char *data, size_t len, 
                               uint8_t qos, bool retain)
{
    if (!s_mqtt_client || !topic || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_stats.is_connected) {
        ESP_LOGW(TAG, "Pas connecté, publication échouera");
        if (s_stats_mutex) {
            xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
            s_stats.publish_errors++;
            xSemaphoreGive(s_stats_mutex);
        }
        return ESP_ERR_INVALID_STATE;
    }
    
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, data, len, qos, retain);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Échec publication topic %s", topic);
        if (s_stats_mutex) {
            xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
            s_stats.publish_errors++;
            xSemaphoreGive(s_stats_mutex);
        }
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Publication: %s (%zu octets)", topic, len);
    return ESP_OK;
}

esp_err_t mqtt_manager_publish_json(const char *topic, const char *json_str)
{
    if (!json_str) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return mqtt_manager_publish(topic, json_str, strlen(json_str), 1, false);
}

esp_err_t mqtt_manager_publish_heartbeat(void)
{
    if (!s_mqtt_client) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Construire topic status
    char topic[128];
    snprintf(topic, sizeof(topic), "apru40/gateway/%s/status", s_client_id);
    
    // Construire payload JSON
    char payload[256];
    snprintf(payload, sizeof(payload), 
             "{\"status\":\"online\",\"timestamp\":%lld,\"uptime\":%lu,\"free_heap\":%lu}",
             (long long)(esp_timer_get_time() / 1000000),
             (unsigned long)(esp_timer_get_time() / 1000000),
             (unsigned long)esp_get_free_heap_size());
    
    return mqtt_manager_publish_json(topic, payload);
}

esp_err_t mqtt_manager_get_stats(mqtt_manager_stats_t *stats)
{
    if (!stats || !s_stats_mutex) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
    memcpy(stats, &s_stats, sizeof(mqtt_manager_stats_t));
    xSemaphoreGive(s_stats_mutex);
    
    return ESP_OK;
}

bool mqtt_manager_is_connected(void)
{
    return s_stats.is_connected;
}

/* ============================================================================
 * GESTION CERTIFICATS TLS
 * ============================================================================ */

/**
 * @brief Helper pour lire un fichier texte dans un buffer alloué
 */
static esp_err_t read_file_to_string(const char *path, char **out)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Impossible d'ouvrir %s: %s", path, strerror(errno));
        return ESP_FAIL;
    }
    
    // Obtenir taille fichier
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 10240) { // Max 10KB par certificat
        ESP_LOGE(TAG, "Taille fichier invalide: %ld octets", size);
        fclose(f);
        return ESP_FAIL;
    }
    
    // Allouer buffer (+1 pour null terminator)
    *out = malloc(size + 1);
    if (!*out) {
        ESP_LOGE(TAG, "Échec allocation mémoire (%ld octets)", size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    // Lire contenu
    size_t read_size = fread(*out, 1, size, f);
    (*out)[read_size] = '\0';
    
    fclose(f);
    
    if (read_size != size) {
        ESP_LOGW(TAG, "Lecture partielle: %zu/%ld octets", read_size, size);
    }
    
    ESP_LOGD(TAG, "Fichier lu: %s (%zu octets)", path, read_size);
    return ESP_OK;
}

esp_err_t mqtt_manager_load_certs_from_sd(const char *mount_point)
{
    if (!mount_point) {
        ESP_LOGE(TAG, "Point de montage NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret;
    sdmmc_card_t *card = NULL;
    char *ca_cert = NULL, *client_cert = NULL, *client_key = NULL;
    
    ESP_LOGI(TAG, "🔍 Montage carte SD...");
    
    // Configuration hôte SD
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    
    // Configuration slot SD
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1; // Mode 1-bit (plus compatible)
    
    // Configuration montage FAT
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    // Monter carte SD
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, 
                                   &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "❌ Échec montage carte SD (carte absente ?)");
        } else {
            ESP_LOGE(TAG, "❌ Échec montage SD: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ Carte SD montée: %s (%.2f GB)", 
             card->cid.name,
             (card->csd.capacity * 512.0) / (1024.0 * 1024.0 * 1024.0));
    
    // Construire chemins des certificats
    char ca_path[96], cert_path[96], key_path[96];
    snprintf(ca_path, sizeof(ca_path), "%s/certs/ca.crt", mount_point);
    snprintf(cert_path, sizeof(cert_path), "%s/certs/client.crt", mount_point);
    snprintf(key_path, sizeof(key_path), "%s/certs/client.key", mount_point);
    
    ESP_LOGI(TAG, "📂 Lecture certificats depuis /certs/...");
    
    // Lire certificat CA
    ret = read_file_to_string(ca_path, &ca_cert);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Échec lecture CA");
        goto cleanup;
    }
    ESP_LOGI(TAG, "  ✓ CA certificate");
    
    // Lire certificat client
    ret = read_file_to_string(cert_path, &client_cert);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Échec lecture certificat client");
        goto cleanup;
    }
    ESP_LOGI(TAG, "  ✓ Client certificate");
    
    // Lire clé privée
    ret = read_file_to_string(key_path, &client_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Échec lecture clé privée");
        goto cleanup;
    }
    ESP_LOGI(TAG, "  ✓ Client private key");
    
    // Ouvrir namespace NVS (chiffré si CONFIG_NVS_ENCRYPTION activé)
    nvs_handle_t nvs_handle;
    ret = nvs_open(NVS_NAMESPACE_CERTS, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Échec ouverture NVS: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "💾 Stockage en NVS chiffré...");
    
    // Stocker certificats en NVS
    ret = nvs_set_str(nvs_handle, "ca_cert", ca_cert);
    ret |= nvs_set_str(nvs_handle, "client_cert", client_cert);
    ret |= nvs_set_str(nvs_handle, "client_key", client_key);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Échec écriture NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        goto cleanup;
    }
    
    // Valider écriture
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Échec commit NVS: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "✅ Certificats stockés en NVS sécurisé");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  🎉 PROVISIONING TERMINÉ AVEC SUCCÈS !   ║");
    ESP_LOGI(TAG, "║                                           ║");
    ESP_LOGI(TAG, "║  💡 Vous pouvez maintenant :             ║");
    ESP_LOGI(TAG, "║     1. RETIRER la carte SD                ║");
    ESP_LOGI(TAG, "║     2. Stocker la SD en lieu sûr          ║");
    ESP_LOGI(TAG, "║     3. Redémarrer l'ESP32                 ║");
    ESP_LOGI(TAG, "║                                           ║");
    ESP_LOGI(TAG, "║  🔒 Les certificats restent en NVS        ║");
    ESP_LOGI(TAG, "║     (chiffré si NVS Encryption activé)    ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
cleanup:
    // Libérer buffers temporaires
    if (ca_cert) free(ca_cert);
    if (client_cert) free(client_cert);
    if (client_key) free(client_key);
    
    // Démonter carte SD
    if (card) {
        esp_vfs_fat_sdcard_unmount(mount_point, card);
        ESP_LOGI(TAG, "📤 Carte SD démontée (retrait sécurisé OK)");
    }
    
    return ret;
}

esp_err_t mqtt_manager_load_certs_from_nvs(char **ca_out, 
                                           char **cert_out, 
                                           char **key_out)
{
    if (!ca_out || !cert_out || !key_out) {
        ESP_LOGE(TAG, "Pointeurs de sortie NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_CERTS, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "⚠️  Aucun certificat en NVS");
            ESP_LOGI(TAG, "💡 Insérer carte SD avec /certs/ pour provisioning");
        } else {
            ESP_LOGE(TAG, "Échec ouverture NVS: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    
    // Obtenir tailles des certificats
    size_t ca_len = 0, cert_len = 0, key_len = 0;
    
    ret = nvs_get_str(nvs_handle, "ca_cert", NULL, &ca_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CA cert non trouvé en NVS");
        nvs_close(nvs_handle);
        return ret;
    }
    
    ret = nvs_get_str(nvs_handle, "client_cert", NULL, &cert_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Client cert non trouvé en NVS");
        nvs_close(nvs_handle);
        return ret;
    }
    
    ret = nvs_get_str(nvs_handle, "client_key", NULL, &key_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Client key non trouvé en NVS");
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Allouer buffers
    *ca_out = malloc(ca_len);
    *cert_out = malloc(cert_len);
    *key_out = malloc(key_len);
    
    if (!*ca_out || !*cert_out || !*key_out) {
        ESP_LOGE(TAG, "Échec allocation mémoire certificats");
        if (*ca_out) free(*ca_out);
        if (*cert_out) free(*cert_out);
        if (*key_out) free(*key_out);
        nvs_close(nvs_handle);
        return ESP_ERR_NO_MEM;
    }
    
    // Lire certificats depuis NVS
    nvs_get_str(nvs_handle, "ca_cert", *ca_out, &ca_len);
    nvs_get_str(nvs_handle, "client_cert", *cert_out, &cert_len);
    nvs_get_str(nvs_handle, "client_key", *key_out, &key_len);
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "✅ Certificats chargés depuis NVS");
    ESP_LOGD(TAG, "   CA: %zu octets", ca_len - 1);
    ESP_LOGD(TAG, "   Cert: %zu octets", cert_len - 1);
    ESP_LOGD(TAG, "   Key: %zu octets", key_len - 1);
    
    return ESP_OK;
}

esp_err_t mqtt_manager_load_certs(char **ca_out,
                                  char **cert_out,
                                  char **key_out)
{
    ESP_LOGI(TAG, "🔐 Chargement certificats TLS...");
    
    // Tentative 1 : Charger depuis NVS
    esp_err_t ret = mqtt_manager_load_certs_from_nvs(ca_out, cert_out, key_out);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Certificats OK (depuis NVS)");
        return ESP_OK;
    }
    
    // Tentative 2 : Charger depuis carte SD et stocker en NVS
    ESP_LOGI(TAG, "🔍 Tentative chargement depuis carte SD...");
    ret = mqtt_manager_load_certs_from_sd("/sdcard");
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Impossible de charger certificats");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "📋 Actions requises :");
        ESP_LOGI(TAG, "   1. Préparer carte SD FAT32");
        ESP_LOGI(TAG, "   2. Créer dossier /certs/");
        ESP_LOGI(TAG, "   3. Copier ca.crt, client.crt, client.key");
        ESP_LOGI(TAG, "   4. Insérer carte dans ESP32");
        ESP_LOGI(TAG, "   5. Redémarrer");
        ESP_LOGI(TAG, "");
        return ret;
    }
    
    // Réessayer lecture depuis NVS (maintenant rempli)
    ret = mqtt_manager_load_certs_from_nvs(ca_out, cert_out, key_out);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Certificats OK (provisioning SD terminé)");
    }
    
    return ret;
}

esp_err_t mqtt_manager_clear_certs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_CERTS, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Échec ouverture NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Effacer toutes les clés du namespace
    ret = nvs_erase_all(nvs_handle);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Certificats effacés du NVS");
    } else {
        ESP_LOGE(TAG, "Échec effacement: %s", esp_err_to_name(ret));
    }
    
    return ret;
}
