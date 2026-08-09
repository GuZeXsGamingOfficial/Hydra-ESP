#include "wifi_controller.h"

#include <stdio.h>
#include <string.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "nvs_flash.h"
#include "nvs.h"

// Tambahan pustaka generator acak hardware ESP32
#include "esp_random.h"
// Pustaka ADC untuk True Random Entropy
#include "driver/adc.h"

static const char* TAG = "wifi_controller";

// Variabel global untuk menyimpan channel AP yang aktif
uint8_t current_ap_channel = 0;

/**
 * @brief Stores current state of Wi-Fi interface
 */
static bool wifi_init = false;
static uint8_t original_mac_ap[6];

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data){

}

/**
 * @brief Menghasilkan LAA MAC Address secara matematis dinamis.
 *        Memilih satu dari 100 array prefix anonim yang valid (suku kedua byte pertama selalu 2/A),
 *        sedangkan 3 byte sisanya diacak total menggunakan hardware ESP32.
 */
void wifictl_set_random_laa_mac() {
    uint8_t current_mac[6];
    
    // Array Statis 100 MAC Prefix Anonim (Semua Byte Pertama Berakhiran 2 atau A)
    static const uint8_t private_mac_prefixes[100][3] = {
        {0x02, 0xAF, 0x12}, {0x1A, 0x2B, 0x3C}, {0x42, 0x5D, 0x6E}, {0xFA, 0x89, 0xAB}, {0xD2, 0x4B, 0x2C},
        {0x22, 0x11, 0x33}, {0x3A, 0x44, 0x55}, {0x52, 0x66, 0x77}, {0x6A, 0x88, 0x99}, {0x72, 0xAA, 0xBB},
        {0x8A, 0xCC, 0xDD}, {0x92, 0xEE, 0xFF}, {0xAA, 0x12, 0x34}, {0xB2, 0x56, 0x78}, {0xCA, 0x9A, 0xBC},
        {0xE2, 0xDE, 0xF0}, {0x0A, 0x13, 0x57}, {0x12, 0x24, 0x68}, {0x2A, 0x35, 0x79}, {0x32, 0x46, 0x8A},
        {0x4A, 0x57, 0x9B}, {0x5A, 0x68, 0xAC}, {0x62, 0x79, 0xBD}, {0x7A, 0x8A, 0xCE}, {0x82, 0x9B, 0xDF},
        {0x9A, 0xAC, 0xE0}, {0xA2, 0xBD, 0xF1}, {0xBA, 0xCE, 0x02}, {0xC2, 0xDF, 0x13}, {0xDA, 0xF0, 0x24},
        {0xEA, 0x01, 0x35}, {0xF2, 0x12, 0x46}, {0x02, 0x23, 0x57}, {0x1A, 0x34, 0x68}, {0x22, 0x45, 0x79},
        {0x3A, 0x56, 0x8A}, {0x42, 0x67, 0x9B}, {0x52, 0x78, 0xAC}, {0x6A, 0x89, 0xBD}, {0x72, 0x9A, 0xCE},
        {0x8A, 0xAB, 0xDF}, {0x92, 0xBC, 0xE0}, {0xAA, 0xCD, 0xF1}, {0xB2, 0xDE, 0x02}, {0xCA, 0xEF, 0x13},
        {0xD2, 0xF0, 0x24}, {0xE2, 0x01, 0x35}, {0xFA, 0x12, 0x46}, {0x0A, 0x23, 0x57}, {0x12, 0x34, 0x68},
        {0x2A, 0x45, 0x79}, {0x32, 0x56, 0x8A}, {0x4A, 0x67, 0x9B}, {0x5A, 0x78, 0xAC}, {0x62, 0x89, 0xBD},
        {0x7A, 0x9A, 0xCE}, {0x82, 0xAB, 0xDF}, {0x9A, 0xBC, 0xE0}, {0xA2, 0xCD, 0xF1}, {0xBA, 0xDE, 0x02},
        {0xC2, 0xEF, 0x13}, {0xDA, 0xF0, 0x24}, {0xEA, 0x01, 0x35}, {0xF2, 0x12, 0x46}, {0x02, 0x23, 0x57},
        {0x1A, 0x34, 0x68}, {0x22, 0x45, 0x79}, {0x3A, 0x56, 0x8A}, {0x42, 0x67, 0x9B}, {0x52, 0x78, 0xAC},
        {0x6A, 0x89, 0xBD}, {0x72, 0x9A, 0xCE}, {0x8A, 0xAB, 0xDF}, {0x92, 0xBC, 0xE0}, {0xAA, 0xCD, 0xF1},
        {0xB2, 0xDE, 0x02}, {0xCA, 0xEF, 0x13}, {0xD2, 0xF0, 0x24}, {0xE2, 0x01, 0x35}, {0xFA, 0x12, 0x46},
        {0x0A, 0x23, 0x57}, {0x12, 0x34, 0x68}, {0x2A, 0x45, 0x79}, {0x32, 0x56, 0x8A}, {0x4A, 0x67, 0x9B},
        {0x5A, 0x78, 0xAC}, {0x62, 0x89, 0xBD}, {0x7A, 0x9A, 0xCE}, {0x82, 0xAB, 0xDF}, {0x9A, 0xBC, 0xE0},
        {0xA2, 0xCD, 0xF1}, {0xBA, 0xDE, 0x02}, {0xC2, 0xEF, 0x13}, {0xDA, 0xF0, 0x24}, {0xEA, 0x01, 0x35},
        {0xF2, 0x12, 0x46}, {0x02, 0x77, 0x88}, {0x1A, 0x99, 0xAA}, {0x22, 0xBB, 0xCC}, {0x3A, 0xDD, 0xEE}
    };

    // 1. Ambil MAC Address interface AP yang saat ini sedang aktif
    if (esp_wifi_get_mac(WIFI_IF_AP, current_mac) == ESP_OK) {
        
        // 2. Pilih salah satu dari 100 Prefix secara acak
        uint32_t random_prefix_index = esp_random() % 100;
        current_mac[0] = private_mac_prefixes[random_prefix_index][0];
        current_mac[1] = private_mac_prefixes[random_prefix_index][1];
        current_mac[2] = private_mac_prefixes[random_prefix_index][2];
        
        // 3. Acak 3 Byte sisanya secara penuh menggunakan hardware generator ESP32
        current_mac[3] = esp_random() % 256;
        current_mac[4] = esp_random() % 256;
        current_mac[5] = esp_random() % 256;
        
        // 4. Terapkan MAC Address LAA baru ke sistem wifi
        esp_err_t err = esp_wifi_set_mac(WIFI_IF_AP, current_mac);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "LAA MAC sukses diacak (Private Address) -> %02X:%02X:%02X:%02X:%02X:%02X",
                     current_mac[0], current_mac[1], current_mac[2], 
                     current_mac[3], current_mac[4], current_mac[5]);
        } else {
            ESP_LOGE(TAG, "Gagal mengubah MAC Address ke LAA (Error: %s)", esp_err_to_name(err));
        }
    }
}

/**
 * @brief Initializes Wi-Fi interface into APSTA mode and starts it.
 *
 * @attention This function should be called only once.
 */
static void wifi_init_apsta(){
    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, original_mac_ap));

    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_init = true;
}

void wifictl_ap_start(wifi_config_t *wifi_config) {
    ESP_LOGD(TAG, "Starting AP...");
    if(!wifi_init){
        wifi_init_apsta();
    }

    // Jika channel belum diatur (bernilai 0 saat booting), gunakan True Random (ADC Noise)
    if (current_ap_channel == 0) {
        // 1. Ambil kebisingan voltase (noise) dari pin ADC1_CHANNEL_0
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
        int noise_seed = adc1_get_raw(ADC1_CHANNEL_0);

        // 2. Kombinasikan noise ADC dengan esp_random() untuk entropi murni
        uint32_t true_random_value = esp_random() + noise_seed;

        // 3. Batasi hasil angka acak agar selalu berada di rentang Channel 1 sampai Channel 11
        current_ap_channel = (true_random_value % 11) + 1; 

        // Log ke serial monitor
        ESP_LOGI(TAG, "[WIFI CONTROL] True Random Channel Berhasil Dihasilkan: CH %d (Noise Seed: %d)", current_ap_channel, noise_seed);
    }
    
    // Terapkan channel acak ke konfigurasi Wi-Fi
    wifi_config->ap.channel = current_ap_channel;

    // Memasukkan konfigurasi wifi ke hardware AP
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, wifi_config));
    
    // Menjalankan fungsi pengacak LAA MAC Address
    wifictl_set_random_laa_mac();

    ESP_LOGI(TAG, "AP started with SSID=%s, Channel=%d", wifi_config->ap.ssid, wifi_config->ap.channel);
}

void wifictl_ap_stop(){
    ESP_LOGD(TAG, "Stopping AP...");
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 0
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_LOGD(TAG, "AP stopped");
}


void wifictl_sta_connect_to_ap(const wifi_ap_record_t *ap_record, const char password[]){
    ESP_LOGD(TAG, "Connecting STA to AP...");
    if(!wifi_init){
        wifi_init_apsta();
    }

    wifi_config_t sta_wifi_config = {
        .sta = {
            .channel = ap_record->primary,
            .scan_method = WIFI_FAST_SCAN,
            .pmf_cfg.capable = false,
            .pmf_cfg.required = false
        },
    };
    memcpy(sta_wifi_config.sta.ssid, ap_record->ssid, 32);

    if(password != NULL){
        if(strlen(password) >= 64) {
            ESP_LOGE(TAG, "Password is too long. Max supported length is 64");
            return;
        }
        memcpy(sta_wifi_config.sta.password, password, strlen(password) + 1);
    }

    ESP_LOGD(TAG, ".ssid=%s", sta_wifi_config.sta.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

}

void wifictl_sta_disconnect(){
    ESP_ERROR_CHECK(esp_wifi_disconnect());
}

void wifictl_set_ap_mac(const uint8_t *mac_ap){
    ESP_LOGD(TAG, "Changing AP MAC address...");
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_AP, mac_ap));
}

void wifictl_get_ap_mac(uint8_t *mac_ap){
    esp_wifi_get_mac(WIFI_IF_AP, mac_ap);
}

void wifictl_restore_ap_mac(){
    ESP_LOGD(TAG, "Restoring original AP MAC address...");
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_AP, original_mac_ap));
}

void wifictl_get_sta_mac(uint8_t *mac_sta){
    esp_wifi_get_mac(WIFI_IF_STA, mac_sta);
}

void wifictl_set_channel(uint8_t channel){
    if((channel == 0) || (channel >  13)){
        ESP_LOGE(TAG,"Channel out of range. Expected value from <1,13> but got %u", channel);
        return;
    }
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

esp_err_t wifictl_get_mgmt_creds(char *out_ssid, size_t ssid_len, char *out_password, size_t password_len) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        // Try to read ssid
        size_t required = ssid_len;
        esp_err_t r = nvs_get_str(nvs, "mgmt_ssid", out_ssid, &required);
        if (r == ESP_OK) {
            if (required == 0) {
                // empty value -> fallback
                strncpy(out_ssid, CONFIG_MGMT_AP_SSID, ssid_len);
                out_ssid[ssid_len - 1] = '\0';
            }
        } else if (r == ESP_ERR_NVS_NOT_FOUND) {
            strncpy(out_ssid, CONFIG_MGMT_AP_SSID, ssid_len);
            out_ssid[ssid_len - 1] = '\0';
        } else if (r == ESP_ERR_NVS_INVALID_LENGTH) {
            // buffer too small: ensure truncation
            // attempt to read required length then truncate
            size_t actual = 0;
            nvs_get_str(nvs, "mgmt_ssid", NULL, &actual);
            if (actual > 0) {
                size_t to_copy = ssid_len - 1;
                nvs_get_str(nvs, "mgmt_ssid", out_ssid, &required);
                out_ssid[to_copy] = '\0';
            } else {
                strncpy(out_ssid, CONFIG_MGMT_AP_SSID, ssid_len);
                out_ssid[ssid_len - 1] = '\0';
            }
        } else {
            strncpy(out_ssid, CONFIG_MGMT_AP_SSID, ssid_len);
            out_ssid[ssid_len - 1] = '\0';
        }

        // Try to read password
        required = password_len;
        r = nvs_get_str(nvs, "mgmt_password", out_password, &required);
        if (r == ESP_OK) {
            if (required == 0) {
                strncpy(out_password, CONFIG_MGMT_AP_PASSWORD, password_len);
                out_password[password_len - 1] = '\0';
            }
        } else if (r == ESP_ERR_NVS_NOT_FOUND) {
            strncpy(out_password, CONFIG_MGMT_AP_PASSWORD, password_len);
            out_password[password_len - 1] = '\0';
        } else if (r == ESP_ERR_NVS_INVALID_LENGTH) {
            size_t actual = 0;
            nvs_get_str(nvs, "mgmt_password", NULL, &actual);
            if (actual > 0) {
                size_t to_copy = password_len - 1;
                nvs_get_str(nvs, "mgmt_password", out_password, &required);
                out_password[to_copy] = '\0';
            } else {
                strncpy(out_password, CONFIG_MGMT_AP_PASSWORD, password_len);
                out_password[password_len - 1] = '\0';
            }
        } else {
            strncpy(out_password, CONFIG_MGMT_AP_PASSWORD, password_len);
            out_password[password_len - 1] = '\0';
        }

        nvs_close(nvs);
        return ESP_OK;
    } else {
        // NVS not available -> fallback to build-time defaults
        strncpy(out_ssid, CONFIG_MGMT_AP_SSID, ssid_len);
        out_ssid[ssid_len - 1] = '\0';
        strncpy(out_password, CONFIG_MGMT_AP_PASSWORD, password_len);
        out_password[password_len - 1] = '\0';
        return err;
    }
}


void wifictl_mgmt_ap_start() {
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    char current_ssid[32] = {0};
    char current_pass[64] = {0};
    wifictl_get_mgmt_creds(current_ssid, sizeof(current_ssid), current_pass, sizeof(current_pass));

    wifi_config_t mgmt_wifi_config = {
        .ap = {
            .ssid_len = strlen(current_ssid),
            .channel = CONFIG_MGMT_AP_CHANNEL,
            .max_connection = CONFIG_MGMT_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };
    memcpy(mgmt_wifi_config.ap.ssid, current_ssid, 32);
    memcpy(mgmt_wifi_config.ap.password, current_pass, 64);

    wifictl_ap_start(&mgmt_wifi_config);
}

void wifictl_mgmt_ap_stop(){
    ESP_LOGW(TAG, "Stopping Management AP...");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
}
