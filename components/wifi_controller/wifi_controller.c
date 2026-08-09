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

static const char* TAG = "wifi_controller";
/**
 * @brief Stores current state of Wi-Fi interface
 */
static bool wifi_init = false;
static uint8_t original_mac_ap[6];

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data){

}

/**
 * @brief Mengubah MAC Address AP agar menggunakan salah satu dari 100 OUI Huawei 
 *        secara acak, dengan 3 byte terakhir yang juga selalu diacak secara dinamis.
 */
void wifictl_set_vendor_huawei_random_mac() {
    uint8_t current_mac[6];
    
    // Daftar 100 OUI (Organizationally Unique Identifier) resmi milik Huawei
    static const uint8_t huawei_ouis[100][3] = {
        {0x00, 0x18, 0x82}, {0x00, 0x1E, 0x10}, {0x00, 0x25, 0x9E}, {0x00, 0x46, 0x4B}, {0x00, 0xE0, 0xFC},
        {0x04, 0xBD, 0x70}, {0x08, 0x19, 0xA6}, {0x0C, 0x37, 0xDC}, {0x0C, 0x45, 0xBA}, {0x0C, 0x96, 0xBF},
        {0x10, 0x1B, 0x54}, {0x10, 0x47, 0x80}, {0x10, 0x51, 0x72}, {0x10, 0xC6, 0x1F}, {0x14, 0xB9, 0x68},
        {0x18, 0xC5, 0x8A}, {0x1C, 0x1D, 0x86}, {0x20, 0x08, 0xED}, {0x20, 0x2B, 0xC1}, {0x20, 0xF3, 0xA3},
        {0x24, 0x09, 0x95}, {0x24, 0x69, 0x68}, {0x28, 0x3C, 0xE4}, {0x28, 0x6E, 0xD4}, {0x2C, 0x9D, 0x1E},
        {0x30, 0xF5, 0x36}, {0x34, 0x00, 0xA3}, {0x34, 0x6A, 0xC2}, {0x38, 0xF8, 0x89}, {0x3C, 0xDF, 0xBD},
        {0x40, 0x4D, 0x8E}, {0x44, 0x55, 0xB1}, {0x48, 0x46, 0xFB}, {0x48, 0x62, 0x76}, {0x4C, 0x1F, 0xCC},
        {0x4C, 0x54, 0x99}, {0x4C, 0xB1, 0x6C}, {0x50, 0x9F, 0x27}, {0x54, 0x89, 0x98}, {0x54, 0xA5, 0x1B},
        {0x58, 0x1F, 0x28}, {0x5C, 0x4C, 0xA9}, {0x5C, 0x7D, 0x5E}, {0x60, 0xDE, 0x44}, {0x64, 0x16, 0xF0},
        {0x68, 0x8F, 0x84}, {0x68, 0xA0, 0x3E}, {0x6C, 0x5E, 0x7A}, {0x70, 0x7B, 0xE8}, {0x70, 0xA5, 0x6A},
        {0x74, 0x88, 0x2A}, {0x78, 0x1D, 0xBA}, {0x7C, 0x60, 0x97}, {0x7C, 0xA2, 0x3E}, {0x80, 0x38, 0xBC},
        {0x80, 0xB6, 0x86}, {0x80, 0xD0, 0x9B}, {0x84, 0xA8, 0xE4}, {0x88, 0x53, 0xD4}, {0x88, 0xCE, 0xFA},
        {0x8C, 0x34, 0xFD}, {0x90, 0x17, 0xAC}, {0x90, 0x4E, 0x91}, {0x94, 0x04, 0x9C}, {0x94, 0x77, 0x2B},
        {0x98, 0x3F, 0x9F}, {0x9C, 0x28, 0xB3}, {0xA0, 0x8C, 0xFD}, {0xA4, 0xCA, 0xA0}, {0xA8, 0x9A, 0x9E},
        {0xAC, 0x4E, 0x91}, {0xAC, 0x85, 0x3D}, {0xAC, 0xE8, 0x7B}, {0xB0, 0x5B, 0x67}, {0xB4, 0x15, 0x13},
        {0xB8, 0x2C, 0xA0}, {0xBC, 0x76, 0x70}, {0xC0, 0x70, 0x09}, {0xC4, 0x05, 0x28}, {0xC8, 0xD1, 0x5E},
        {0xCC, 0x53, 0xB5}, {0xCC, 0x96, 0xA0}, {0xD0, 0x2D, 0xB3}, {0xD4, 0x40, 0xF0}, {0xD4, 0x6A, 0xA8},
        {0xD8, 0x49, 0x0B}, {0xDC, 0xD2, 0xFC}, {0xE0, 0x24, 0x81}, {0xE0, 0x97, 0x96}, {0xE4, 0x54, 0xE8},
        {0xE4, 0x68, 0xA3}, {0xE8, 0x08, 0x8B}, {0xE8, 0xCD, 0x2D}, {0xEC, 0x23, 0x3D}, {0xEC, 0xCB, 0x30},
        {0xF4, 0x55, 0x9C}, {0xF4, 0xC7, 0x14}, {0xF8, 0x3D, 0xFF}, {0xF8, 0x4A, 0xBF}, {0xFC, 0x48, 0xEF}
    };
    
    // 1. Ambil MAC Address interface AP yang saat ini sedang aktif
    if (esp_wifi_get_mac(WIFI_IF_AP, current_mac) == ESP_OK) {
        
        // 2. Pilih salah satu index OUI secara acak (0 hingga 99)
        uint32_t random_oui_index = esp_random() % 100;
        
        // 3. Ganti 3 byte pertama dengan OUI Huawei terpilih
        current_mac[0] = huawei_ouis[random_oui_index][0];
        current_mac[1] = huawei_ouis[random_oui_index][1];
        current_mac[2] = huawei_ouis[random_oui_index][2];
        
        // 4. Acak 3 byte terakhir secara mandiri menggunakan modul hardware generator ESP32
        current_mac[3] = esp_random() % 256;
        current_mac[4] = esp_random() % 256;
        current_mac[5] = esp_random() % 256;
        
        // 5. Terapkan MAC Address baru ke sistem wifi
        esp_err_t err = esp_wifi_set_mac(WIFI_IF_AP, current_mac);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Vendor MAC sukses diubah ke HUAWEI Acak -> %02X:%02X:%02X:%02X:%02X:%02X",
                     current_mac[0], current_mac[1], current_mac[2], 
                     current_mac[3], current_mac[4], current_mac[5]);
        } else {
            ESP_LOGE(TAG, "Gagal mengubah MAC Address ke vendor Huawei (Error: %s)", esp_err_to_name(err));
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

    // Memasukkan konfigurasi wifi asli ke hardware AP
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, wifi_config));
    
    // Menjalankan fungsi pengubah identitas vendor Huawei + Acak Penuh
    wifictl_set_vendor_huawei_random_mac();

    ESP_LOGI(TAG, "AP started with SSID=%s", wifi_config->ap.ssid);
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
