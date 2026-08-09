/**
 * @file main.c
 * @author risinek (risinek@gmail.com), SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 2021-04-03
 * @copyright Copyright (c) 2021
 * 
 * @brief Main file used to setup ESP32 into initial state
 * 
 * Starts management AP and webserver  
 */

#include <stdio.h>
#include <string.h>            // Ditambahkan untuk fungsi strlen

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"               // Ditambahkan untuk fungsi baca/tulis NVS
#include "attack.h"
#include "wifi_controller.h"
#include "webserver.h"
#include "hydra_ssd1306_display.h"

static const char* TAG = "main";

void app_main(void)
{
    ESP_LOGD(TAG, "app_main started");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // =========================================================================
    // LOGIKA PENGECEKAN & PENGISIAN CREDENTIALS DEFAULT AP (SUPER BARU - UNLOCKED)
    // =========================================================================
    nvs_handle_t nvs_handle;
    // Membuka namespace "storage" sesuai dengan yang digunakan oleh webserver.c
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        char saved_ssid[33] = {0}; // Buffer untuk menampung SSID (maksimal 32 karakter + null terminator)
        size_t required_size = sizeof(saved_ssid);
        
        // Membaca isi string "ap_ssid" dari memori NVS ke dalam buffer saved_ssid
        esp_err_t err = nvs_get_str(nvs_handle, "ap_ssid", saved_ssid, &required_size);
        
        // Tulis default HANYA jika kunci tidak ditemukan ATAU isi teksnya kosong (0 karakter)
        if (err == ESP_ERR_NVS_NOT_FOUND || strlen(saved_ssid) == 0) {
            ESP_LOGI(TAG, "NVS kosong atau tidak valid, menulis default AP: ssid=hydra, pass=notforfun");
            nvs_set_str(nvs_handle, "ap_ssid", "hydra");
            nvs_set_str(nvs_handle, "ap_pass", "notforfun");
            nvs_commit(nvs_handle);
        } else {
            // Jika sudah ada SSID buatan Anda dari Web UI, biarkan dan jangan ditimpa!
            ESP_LOGI(TAG, "SSID kustom ditemukan di NVS: %s. Menggunakan pengaturan user.", saved_ssid);
        }
        nvs_close(nvs_handle);
    }
    // =========================================================================

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Wi-Fi menyala dan otomatis akan membaca data SSID/Pass terbaru dari NVS namespace "storage"
    wifictl_mgmt_ap_start();
    
    attack_init();

    hydra_display_init();

    webserver_run();
}
