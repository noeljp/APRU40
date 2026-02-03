#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "ads7128.h"
#include "ads1119.h"
#include "tca9537.h"
#include "esp_now_secure.h"
#include "bluetooth_spp.h"

#define TAG "APRU40"

/* ===== Config ===== */
#define I2C_PORT              I2C_NUM_0

#define ADS7128_ADDR          0x17
#define ADS7128_PERIOD_MS     5000
#define ADS7128_CHANNEL_COUNT 8

#define ADS1119_ADDR_1        0x40
#define ADS1119_ADDR_2        0x41
#define ADS1119_PERIOD_MS     5000

#define TCA9537_ADDR          0x49
#define TCA9537_PERIOD_MS     1000

/* ESP-NOW sécurisé */
#define ESP_NOW_NODE_ID       1      // ID unique du nœud (à changer par nœud)
#define ESP_NOW_CHANNEL       0      // Canal WiFi (0=auto)
#define ESP_NOW_TX_PERIOD_MS  10000  // Envoyer données toutes les 10s

/* Clés de sécurité (à personnaliser et garder secrètes !) */
static const uint8_t g_aes_key[32] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    0x76, 0x2e, 0x71, 0x60, 0xf3, 0x8b, 0x4d, 0xa5,
    0x6a, 0x78, 0x4d, 0x90, 0x45, 0x19, 0x0c, 0xfe
};

static const uint8_t g_hmac_key[32] = {
    0xc0, 0x9f, 0xbb, 0xe9, 0x93, 0xb5, 0x3a, 0x4f,
    0x2a, 0x18, 0x91, 0x57, 0x89, 0xcb, 0xad, 0x29,
    0x34, 0x7d, 0xe4, 0x22, 0xf5, 0x90, 0x1b, 0xc6,
    0x88, 0x4f, 0x32, 0xe1, 0x7a, 0x2b, 0xf9, 0x63
};

/* Scanner Zebra DS2278 (Bluetooth SPP) */
#define BT_SCANNER_NAME       "TAS-MACHINE-QR-01"   // Nom unique par machine
#define BT_SCANNER_PIN        "736281"              // PIN fixe 6+ chiffres

// ⚠️ PAIRING INITIAL : mettre NULL (tous autorisés)
// ⚠️ APRÈS PAIRING : remplacer par l'adresse BT du scanner
static const uint8_t scanner_bt_addr[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};  // À remplacer après pairing

#define BT_PAIRING_MODE       true   // true = pairing initial, false = production

/* ===== Messages envoyés par les tasks d'acquisition ===== */
typedef struct {
    uint32_t seq;
    TickType_t tick;
    uint16_t raw[ADS7128_CHANNEL_COUNT];
    float    v[ADS7128_CHANNEL_COUNT];
} ads7128_frame_t;

typedef struct {
    uint32_t seq;
    TickType_t tick;
    int16_t  raw[4];   // 4 canaux différentiels ou single-ended
    float    v[4];
} ads1119_frame_t;

static QueueHandle_t g_ads7128_q = NULL;
static QueueHandle_t g_ads1119_q1 = NULL;
static QueueHandle_t g_ads1119_q2 = NULL;

static ads7128_t g_ads7128;
static ads1119_t g_ads1119_1;
static ads1119_t g_ads1119_2;
static tca9537_t g_tca9537;

/* =========================================================
 * Callbacks ESP-NOW sécurisé
 * ========================================================= */
static void esp_now_recv_callback(const uint8_t *sender_mac, const uint8_t *data, uint8_t len, int8_t rssi) {
    ESP_LOGI(TAG, "ESP-NOW RX de %02X:%02X:%02X:%02X:%02X:%02X: %d octets, RSSI=%d dBm",
             sender_mac[0], sender_mac[1], sender_mac[2], sender_mac[3], sender_mac[4], sender_mac[5],
             len, rssi);
    
    // Afficher le contenu si c'est du texte
    if (len > 0 && len < 200) {
        char buf[201] = {0};
        memcpy(buf, data, len);
        buf[len] = '\0';
        ESP_LOGI(TAG, "Contenu: %s", buf);
    }
    
    // Traitement des commandes reçues
    if (len > 0) {
        // Exemple: commande "LED_ON" / "LED_OFF"
        if (strncmp((char*)data, "LED_ON", 6) == 0) {
            ESP_LOGI(TAG, "Commande reçue: LED_ON");
            tca9537_set_pin(&g_tca9537, TCA9537_PIN1);
        } else if (strncmp((char*)data, "LED_OFF", 7) == 0) {
            ESP_LOGI(TAG, "Commande reçue: LED_OFF");
            tca9537_clear_pin(&g_tca9537, TCA9537_PIN1);
        }
    }
}

static void esp_now_send_callback(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "ESP-NOW TX: succès");
    } else {
        ESP_LOGW(TAG, "ESP-NOW TX: échec");
    }
}

/* =========================================================
 * Callbacks Bluetooth SPP (Scanner Zebra)
 * ========================================================= */
static void scanner_data_callback(const char *line, size_t len) {
    ESP_LOGI(TAG, "Scanner RX: %s (len=%d)", line, len);
    
    // Validation longueur
    if (len < 5 || len > 200) {
        ESP_LOGW(TAG, "Ligne invalide (longueur)");
        return;
    }
    
    // Validation préfixe et traitement
    if (strncmp(line, "TAS|", 4) == 0) {
        // Données format TAS (ex: TAS|MACHINE|TVAC09|TRAY|A17)
        ESP_LOGI(TAG, "Format TAS détecté");
        // TODO: Parser et traiter données TAS
        
    } else if (strncmp(line, "EAN13|", 6) == 0) {
        // Code EAN13
        ESP_LOGI(TAG, "EAN13: %s", line + 6);
        // TODO: Traiter code EAN
        
    } else if (strncmp(line, "QR|", 3) == 0) {
        // QR Code
        ESP_LOGI(TAG, "QR Code: %s", line + 3);
        // TODO: Traiter QR code
        
    } else {
        // Format générique (code-barres brut)
        ESP_LOGI(TAG, "Code-barres brut: %s", line);
        // TODO: Traiter code générique
    }
    
    // Envoyer accusé réception au scanner (optionnel)
    char ack[] = "OK\r\n";
    bt_spp_send((uint8_t*)ack, strlen(ack));
}

static void scanner_conn_callback(bool connected, const uint8_t *remote_addr) {
    if (connected) {
        ESP_LOGI(TAG, "Scanner connecté: %02X:%02X:%02X:%02X:%02X:%02X",
                 remote_addr[0], remote_addr[1], remote_addr[2],
                 remote_addr[3], remote_addr[4], remote_addr[5]);
        
        // Allumer LED pour indiquer connexion
        tca9537_set_pin(&g_tca9537, TCA9537_PIN1);
    } else {
        ESP_LOGI(TAG, "Scanner déconnecté");
        
        // Éteindre LED
        tca9537_clear_pin(&g_tca9537, TCA9537_PIN1);
    }
}

/* =========================================================
 * Task 1: Heartbeat
 * ========================================================= */
static void task_heartbeat(void *arg)
{
    (void)arg;
    while (1) {
        printf(".");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* =========================================================
 * Task 2: ADS7128 acquisition
 * ========================================================= */
static void task_ads7128_acq(void *arg)
{
    (void)arg;

    ads7128_frame_t frame = {0};
    frame.seq = 0;

    ESP_LOGI(TAG, "ADS7128 acquisition task started");

    while (1) {
        frame.seq++;
        frame.tick = xTaskGetTickCount();

        for (uint8_t ch = 0; ch < ADS7128_CHANNEL_COUNT; ch++) {
            uint16_t raw = 0;

            esp_err_t err = ads7128_set_adc_channel(&g_ads7128, ch);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "ADS7128 set ch%d failed: %s", ch, esp_err_to_name(err));
                raw = 0;
            } else {
                err = ads7128_read_adc_raw12(&g_ads7128, &raw);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "ADS7128 read ch%d failed: %s", ch, esp_err_to_name(err));
                    raw = 0;
                }
            }

            frame.raw[ch] = raw;
            frame.v[ch]   = ads7128_raw12_to_volts(&g_ads7128, raw);

            vTaskDelay(pdMS_TO_TICKS(10)); // mini pause entre canaux
        }

        // Envoi non bloquant (on garde toujours la dernière mesure)
        xQueueOverwrite(g_ads7128_q, &frame);

        vTaskDelay(pdMS_TO_TICKS(ADS7128_PERIOD_MS));
    }
}

/* =========================================================
 * Task 3: ADS1119 #1 acquisition (0x40)
 * ========================================================= */
static void task_ads1119_acq_1(void *arg)
{
    (void)arg;

    ads1119_frame_t frame = {0};
    frame.seq = 0;

    ESP_LOGI(TAG, "ADS1119 #1 (0x40) acquisition task started");

    while (1) {
        frame.seq++;
        frame.tick = xTaskGetTickCount();

        for (uint8_t ch = 0; ch < 4; ch++) {
            int16_t raw = 0;

            // Sélection du canal (API cohérente avec ADS7128)
            esp_err_t err = ads1119_set_channel(&g_ads1119_1, ch);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "ADS1119#1 set ch%d failed: %s", ch, esp_err_to_name(err));
                raw = 0;
            } else {
                // Démarrer conversion
                err = ads1119_start_conversion(&g_ads1119_1);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "ADS1119#1 start ch%d failed: %s", ch, esp_err_to_name(err));
                    raw = 0;
                } else {
                    // Attendre conversion (typique: 90ms pour single-shot)
                    vTaskDelay(pdMS_TO_TICKS(100));

                    // Lire résultat
                    err = ads1119_read_raw16(&g_ads1119_1, &raw);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "ADS1119#1 read ch%d failed: %s", ch, esp_err_to_name(err));
                        raw = 0;
                    }
                }
            }

            frame.raw[ch] = raw;
            frame.v[ch]   = ads1119_raw16_to_volts(&g_ads1119_1, raw);
        }

        // Envoi non bloquant
        xQueueOverwrite(g_ads1119_q1, &frame);

        vTaskDelay(pdMS_TO_TICKS(ADS1119_PERIOD_MS));
    }
}

/* =========================================================
 * Task 4: ADS1119 #2 acquisition (0x41)
 * ========================================================= */
static void task_ads1119_acq_2(void *arg)
{
    (void)arg;

    ads1119_frame_t frame = {0};
    frame.seq = 0;

    ESP_LOGI(TAG, "ADS1119 #2 (0x41) acquisition task started");

    while (1) {
        frame.seq++;
        frame.tick = xTaskGetTickCount();

        for (uint8_t ch = 0; ch < 4; ch++) {
            int16_t raw = 0;

            // Sélection du canal (API cohérente avec ADS7128)
            esp_err_t err = ads1119_set_channel(&g_ads1119_2, ch);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "ADS1119#2 set ch%d failed: %s", ch, esp_err_to_name(err));
                raw = 0;
            } else {
                // Démarrer conversion
                err = ads1119_start_conversion(&g_ads1119_2);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "ADS1119#2 start ch%d failed: %s", ch, esp_err_to_name(err));
                    raw = 0;
                } else {
                    // Attendre conversion (typique: 90ms pour single-shot)
                    vTaskDelay(pdMS_TO_TICKS(100));

                    // Lire résultat
                    err = ads1119_read_raw16(&g_ads1119_2, &raw);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "ADS1119#2 read ch%d failed: %s", ch, esp_err_to_name(err));
                        raw = 0;
                    }
                }
            }

            frame.raw[ch] = raw;
            frame.v[ch]   = ads1119_raw16_to_volts(&g_ads1119_2, raw);
        }

        // Envoi non bloquant
        xQueueOverwrite(g_ads1119_q2, &frame);

        vTaskDelay(pdMS_TO_TICKS(ADS1119_PERIOD_MS));
    }
}

/* =========================================================
 * Task 5: Logger ADS7128
 * ========================================================= */
static void task_ads7128_log(void *arg)
{
    (void)arg;
    ads7128_frame_t frame;

    ESP_LOGI(TAG, "ADS7128 logger task started");

    while (1) {
        // attend la dernière frame
        if (xQueueReceive(g_ads7128_q, &frame, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "---- ADS7128 frame #%lu (tick=%lu) ----",
                     (unsigned long)frame.seq, (unsigned long)frame.tick);

            for (uint8_t ch = 0; ch < ADS7128_CHANNEL_COUNT; ch++) {
                ESP_LOGI(TAG, "  CH%d: raw=%4u  %.3f V",
                         ch, (unsigned)frame.raw[ch], frame.v[ch]);
            }
        }
    }
}

/* =========================================================
 * Task 6: Logger ADS1119 #1
 * ========================================================= */
static void task_ads1119_log_1(void *arg)
{
    (void)arg;
    ads1119_frame_t frame;

    ESP_LOGI(TAG, "ADS1119 #1 logger task started");

    while (1) {
        if (xQueueReceive(g_ads1119_q1, &frame, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "---- ADS1119 #1 (0x40) frame #%lu (tick=%lu) ----",
                     (unsigned long)frame.seq, (unsigned long)frame.tick);

            for (uint8_t ch = 0; ch < 4; ch++) {
                ESP_LOGI(TAG, "  CH%d: raw=%6d  %+.4f V",
                         ch, frame.raw[ch], frame.v[ch]);
            }
        }
    }
}

/* =========================================================
 * Task 7: Logger ADS1119 #2
 * ========================================================= */
static void task_ads1119_log_2(void *arg)
{
    (void)arg;
    ads1119_frame_t frame;

    ESP_LOGI(TAG, "ADS1119 #2 logger task started");

    while (1) {
        if (xQueueReceive(g_ads1119_q2, &frame, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "---- ADS1119 #2 (0x41) frame #%lu (tick=%lu) ----",
                     (unsigned long)frame.seq, (unsigned long)frame.tick);

            for (uint8_t ch = 0; ch < 4; ch++) {
                ESP_LOGI(TAG, "  CH%d: raw=%6d  %+.4f V",
                         ch, frame.raw[ch], frame.v[ch]);
            }
        }
    }
}

/* =========================================================
 * Task 8: TCA9537 control (GPIO expander)
 * ========================================================= */
static void task_tca9537_ctrl(void *arg)
{
    (void)arg;
    
    ESP_LOGI(TAG, "TCA9537 control task started");
    
    // Configuration: PIN0 en input, PIN1-2-3 en output
    esp_err_t err = tca9537_set_pin_mode(&g_tca9537, TCA9537_PIN0, true);  // PIN0 = input
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TCA9537 config PIN0 input failed: %s", esp_err_to_name(err));
    }
    
    err = tca9537_set_pin_mode(&g_tca9537, TCA9537_PIN1 | TCA9537_PIN2 | TCA9537_PIN3, false);  // PIN1-2-3 = output
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TCA9537 config failed: %s", esp_err_to_name(err));
    }
    
    uint8_t pin_state = 0;
    
    while (1) {
        // Toggle PIN0
        err = tca9537_toggle_pin(&g_tca9537, TCA9537_PIN0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "TCA9537 toggle failed: %s", esp_err_to_name(err));
        }
        
        // Lire l'état des pins
        err = tca9537_read_pins(&g_tca9537, &pin_state);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "TCA9537 pins state: 0x%02X", pin_state);
        }
        
        vTaskDelay(pdMS_TO_TICKS(TCA9537_PERIOD_MS));
    }
}

/* =========================================================
 * Task 9: ESP-NOW sécurisé - TX périodique
 * ========================================================= */
static void task_esp_now_tx(void *arg)
{
    (void)arg;
    
    ESP_LOGI(TAG, "ESP-NOW TX task started");
    
    uint32_t tx_counter = 0;
    uint8_t local_mac[6];
    esp_now_secure_get_local_mac(local_mac);
    
    ESP_LOGI(TAG, "MAC locale: %02X:%02X:%02X:%02X:%02X:%02X",
             local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);
    
    while (1) {
        // Préparer données capteurs à envoyer
        char tx_msg[150];
        int len = snprintf(tx_msg, sizeof(tx_msg),
                          "{\"node\":%d,\"seq\":%lu,\"type\":\"sensor_data\"}",
                          ESP_NOW_NODE_ID, (unsigned long)tx_counter++);
        
        // Envoyer via ESP-NOW sécurisé
        esp_err_t err = esp_now_secure_send((uint8_t*)tx_msg, len);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Données envoyées: %s", tx_msg);
        } else {
            ESP_LOGE(TAG, "Erreur envoi ESP-NOW: %s", esp_err_to_name(err));
        }
        
        // Afficher statistiques de sécurité
        uint32_t valid, invalid_hmac, replay, untrusted;
        esp_now_secure_get_stats(&valid, &invalid_hmac, &replay, &untrusted);
        ESP_LOGI(TAG, "Stats sécurité - Valid:%lu InvalidHMAC:%lu Replay:%lu Untrusted:%lu",
                 valid, invalid_hmac, replay, untrusted);
        
        vTaskDelay(pdMS_TO_TICKS(ESP_NOW_TX_PERIOD_MS));
    }
}

/* =========================================================
 * app_main
 * ========================================================= */
void app_main(void)
{
    i2c_bus_init();
    ESP_LOGI(TAG, "RTOS start");

    // 1) Init ADS7128 (SIM ou REAL selon build flag)
    ESP_ERROR_CHECK(ads7128_init(&g_ads7128, I2C_PORT, ADS7128_ADDR));
    ads7128_set_vmax(&g_ads7128, 3.3f);

    // 2) Init ADS1119 #1 (0x40)
    ESP_ERROR_CHECK(ads1119_init(&g_ads1119_1, I2C_PORT, ADS1119_ADDR_1));
    ads1119_set_vref(&g_ads1119_1, 2.048f);

    // 3) Init ADS1119 #2 (0x41)
    ESP_ERROR_CHECK(ads1119_init(&g_ads1119_2, I2C_PORT, ADS1119_ADDR_2));
    ads1119_set_vref(&g_ads1119_2, 2.048f);

    // 4) Init TCA9537 (0x49)
    ESP_ERROR_CHECK(tca9537_init(&g_tca9537, I2C_PORT, TCA9537_ADDR));

    // 5) Init Bluetooth SPP (Scanner Zebra DS2278)
    bt_spp_config_t bt_config = {
        .device_name = BT_SCANNER_NAME,
        .pin_code = BT_SCANNER_PIN,
        .discoverable_at_init = BT_PAIRING_MODE,  // true pour pairing initial
        .whitelist_addr = BT_PAIRING_MODE ? NULL : scanner_bt_addr,  // whitelist après pairing
        .data_cb = scanner_data_callback,
        .conn_cb = scanner_conn_callback,
    };
    
    ESP_ERROR_CHECK(bt_spp_init(&bt_config));
    
    uint8_t local_bt_addr[6];
    bt_spp_get_local_addr(local_bt_addr);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "PAIRING BARCODE : <FNC3>B%02X%02X%02X%02X%02X%02X",
             local_bt_addr[0], local_bt_addr[1], local_bt_addr[2],
             local_bt_addr[3], local_bt_addr[4], local_bt_addr[5]);
    ESP_LOGI(TAG, "========================================");

    // 6) Init ESP-NOW sécurisé
    esp_now_secure_config_t esp_now_config = {
        .node_id = ESP_NOW_NODE_ID,
        .channel = ESP_NOW_CHANNEL,
        .recv_cb = esp_now_recv_callback,
        .send_cb = esp_now_send_callback,
    };
    memcpy(esp_now_config.aes_key, g_aes_key, 32);
    memcpy(esp_now_config.hmac_key, g_hmac_key, 32);
    
    ESP_ERROR_CHECK(esp_now_secure_init(&esp_now_config));
    ESP_LOGI(TAG, "ESP-NOW sécurisé initialisé (Node ID: %d)", ESP_NOW_NODE_ID);

    // 6) Create queues (taille 1, overwrite = toujours la dernière mesure)
    g_ads7128_q = xQueueCreate(1, sizeof(ads7128_frame_t));
    g_ads1119_q1 = xQueueCreate(1, sizeof(ads1119_frame_t));
    g_ads1119_q2 = xQueueCreate(1, sizeof(ads1119_frame_t));
    
    if (!g_ads7128_q || !g_ads1119_q1 || !g_ads1119_q2) {
        ESP_LOGE(TAG, "Queue allocation failed");
        return;
    }

    // 7) Create tasks
    xTaskCreate(task_heartbeat,      "heartbeat",   2048, NULL, 1, NULL);
    xTaskCreate(task_ads7128_acq,    "ads7128_acq", 4096, NULL, 6, NULL);
    xTaskCreate(task_ads1119_acq_1,  "ads1119_acq1",4096, NULL, 6, NULL);
    xTaskCreate(task_ads1119_acq_2,  "ads1119_acq2",4096, NULL, 6, NULL);
    xTaskCreate(task_ads7128_log,    "ads7128_log", 4096, NULL, 4, NULL);
    xTaskCreate(task_ads1119_log_1,  "ads1119_log1",4096, NULL, 4, NULL);
    xTaskCreate(task_ads1119_log_2,  "ads1119_log2",4096, NULL, 4, NULL);
    xTaskCreate(task_tca9537_ctrl,   "tca9537_ctrl",2048, NULL, 3, NULL);
    xTaskCreate(task_esp_now_tx,     "esp_now_tx",  4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Toutes les tasks démarrées");
    // app_main peut retourner (les tâches vivent)
}
