#include "bluetooth_spp.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_log.h"
#include <string.h>

#define TAG "BT_SPP"
#define SPP_SERVER_NAME "SPP_SERVER"

static bt_spp_config_t g_config = {0};
static uint32_t g_spp_handle = 0;
static bool g_connected = false;
static uint8_t g_remote_addr[6] = {0};

// Buffer pour réception ligne par ligne
static char g_rx_buffer[BT_SPP_MAX_LINE_LEN];
static size_t g_rx_len = 0;

/* Forward declarations */
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static bool is_whitelisted(const uint8_t *addr);
static void process_received_data(const uint8_t *data, size_t len);

esp_err_t bt_spp_init(const bt_spp_config_t *config) {
    if (!config || !config->device_name || !config->pin_code) {
        ESP_LOGE(TAG, "Configuration invalide");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copier configuration
    memcpy(&g_config, config, sizeof(bt_spp_config_t));
    
    // Initialiser contrôleur Bluetooth
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init BT controller: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Mode BTDM (dual mode) compatible avec sdkconfig CONFIG_BTDM_CTRL_MODE_BTDM
    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur enable BT controller: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialiser Bluedroid
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init Bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur enable Bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Enregistrer callback GAP
    esp_bt_gap_register_callback(esp_bt_gap_cb);
    
    // Configurer le nom Bluetooth
    esp_bt_gap_set_device_name(config->device_name);
    
    // Configurer sécurité : mode 4 = Secure Connections (passkey obligatoire)
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_OUT; // Affichage PIN uniquement
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
    
    // PIN fixe
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    strncpy((char*)pin_code, config->pin_code, ESP_BT_PIN_CODE_LEN);
    esp_bt_gap_set_pin(pin_type, strlen(config->pin_code), pin_code);
    
    // Mode discoverable si demandé
    if (config->discoverable_at_init) {
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    } else {
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    }
    
    // Initialiser SPP
    ret = esp_spp_register_callback(esp_spp_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur register SPP callback: %s", esp_err_to_name(ret));
        return ret;
    }
    
    esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0  // 0 = utiliser valeur par défaut
    };
    ret = esp_spp_enhanced_init(&spp_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur init SPP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Afficher adresse BT locale
    const uint8_t *local_addr = esp_bt_dev_get_address();
    ESP_LOGI(TAG, "Bluetooth SPP initialisé");
    ESP_LOGI(TAG, "Nom: %s", config->device_name);
    ESP_LOGI(TAG, "Adresse BT: %02X:%02X:%02X:%02X:%02X:%02X",
             local_addr[0], local_addr[1], local_addr[2],
             local_addr[3], local_addr[4], local_addr[5]);
    ESP_LOGI(TAG, "PIN: %s", config->pin_code);
    ESP_LOGI(TAG, "Discoverable: %s", config->discoverable_at_init ? "OUI" : "NON");
    
    if (config->whitelist_addr) {
        ESP_LOGI(TAG, "Whitelist: %02X:%02X:%02X:%02X:%02X:%02X",
                 config->whitelist_addr[0], config->whitelist_addr[1], config->whitelist_addr[2],
                 config->whitelist_addr[3], config->whitelist_addr[4], config->whitelist_addr[5]);
    }
    
    return ESP_OK;
}

esp_err_t bt_spp_set_discoverable(bool discoverable) {
    if (discoverable) {
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        ESP_LOGI(TAG, "Mode discoverable activé");
    } else {
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        ESP_LOGI(TAG, "Mode discoverable désactivé");
    }
    return ESP_OK;
}

esp_err_t bt_spp_send(const uint8_t *data, size_t len) {
    if (!g_connected || g_spp_handle == 0) {
        ESP_LOGW(TAG, "SPP non connecté, impossible d'envoyer");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_spp_write(g_spp_handle, len, (uint8_t*)data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erreur SPP write: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t bt_spp_get_local_addr(uint8_t *addr) {
    if (!addr) return ESP_ERR_INVALID_ARG;
    const uint8_t *local_addr = esp_bt_dev_get_address();
    if (!local_addr) return ESP_ERR_INVALID_STATE;
    memcpy(addr, local_addr, 6);
    return ESP_OK;
}

esp_err_t bt_spp_disconnect(void) {
    if (!g_connected || g_spp_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_spp_disconnect(g_spp_handle);
}

void bt_spp_deinit(void) {
    if (g_connected) {
        esp_spp_disconnect(g_spp_handle);
    }
    esp_spp_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    ESP_LOGI(TAG, "Bluetooth SPP désinitialisé");
}

/* ===== Callbacks internes ===== */

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
        case ESP_SPP_INIT_EVT:
            ESP_LOGI(TAG, "SPP initialisé, démarrage serveur...");
            esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
            break;
            
        case ESP_SPP_SRV_OPEN_EVT:
            ESP_LOGI(TAG, "SPP Server ouvert");
            g_spp_handle = param->srv_open.handle;
            memcpy(g_remote_addr, param->srv_open.rem_bda, 6);
            
            // Vérifier whitelist
            if (g_config.whitelist_addr && !is_whitelisted(g_remote_addr)) {
                ESP_LOGW(TAG, "Appareil non autorisé: %02X:%02X:%02X:%02X:%02X:%02X",
                         g_remote_addr[0], g_remote_addr[1], g_remote_addr[2],
                         g_remote_addr[3], g_remote_addr[4], g_remote_addr[5]);
                esp_spp_disconnect(g_spp_handle);
                break;
            }
            
            g_connected = true;
            ESP_LOGI(TAG, "Client SPP connecté: %02X:%02X:%02X:%02X:%02X:%02X",
                     g_remote_addr[0], g_remote_addr[1], g_remote_addr[2],
                     g_remote_addr[3], g_remote_addr[4], g_remote_addr[5]);
            
            if (g_config.conn_cb) {
                g_config.conn_cb(true, g_remote_addr);
            }
            break;
            
        case ESP_SPP_CLOSE_EVT:
            ESP_LOGI(TAG, "Connexion SPP fermée");
            g_connected = false;
            g_spp_handle = 0;
            g_rx_len = 0;  // Reset buffer réception
            
            if (g_config.conn_cb) {
                g_config.conn_cb(false, g_remote_addr);
            }
            break;
            
        case ESP_SPP_DATA_IND_EVT:
            // Données reçues
            process_received_data(param->data_ind.data, param->data_ind.len);
            break;
            
        default:
            break;
    }
}

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Authentification réussie avec %02X:%02X:%02X:%02X:%02X:%02X",
                         param->auth_cmpl.bda[0], param->auth_cmpl.bda[1], param->auth_cmpl.bda[2],
                         param->auth_cmpl.bda[3], param->auth_cmpl.bda[4], param->auth_cmpl.bda[5]);
            } else {
                ESP_LOGE(TAG, "Échec authentification: %d", param->auth_cmpl.stat);
            }
            break;
            
        case ESP_BT_GAP_PIN_REQ_EVT:
            ESP_LOGI(TAG, "Demande PIN...");
            break;
            
        default:
            break;
    }
}

static bool is_whitelisted(const uint8_t *addr) {
    if (!g_config.whitelist_addr) return true;  // Pas de whitelist = tous autorisés
    return memcmp(addr, g_config.whitelist_addr, 6) == 0;
}

static void process_received_data(const uint8_t *data, size_t len) {
    // Traiter les données reçues caractère par caractère
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        
        // Détecter fin de ligne (LF ou CRLF)
        if (c == '\n') {
            // Ligne complète reçue
            g_rx_buffer[g_rx_len] = '\0';
            
            // Supprimer éventuel CR en fin
            if (g_rx_len > 0 && g_rx_buffer[g_rx_len - 1] == '\r') {
                g_rx_buffer[g_rx_len - 1] = '\0';
                g_rx_len--;
            }
            
            // Callback utilisateur
            if (g_rx_len > 0 && g_config.data_cb) {
                g_config.data_cb(g_rx_buffer, g_rx_len);
            }
            
            // Reset buffer
            g_rx_len = 0;
        } else if (c != '\r') {  // Ignorer CR (géré avec LF)
            // Ajouter au buffer si place disponible
            if (g_rx_len < BT_SPP_MAX_LINE_LEN - 1) {
                g_rx_buffer[g_rx_len++] = c;
            } else {
                ESP_LOGW(TAG, "Buffer ligne plein, ligne tronquée");
                g_rx_len = 0;  // Reset
            }
        }
    }
}
