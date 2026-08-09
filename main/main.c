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
    // LOGIKA PENGECEKAN & PENGISIAN CREDENTIALS DEFAULT AP (UNLOCKED)
    // =========================================================================
    nvs_handle_t nvs_handle;
    // Membuka namespace "storage" sesuai dengan yang digunakan oleh webserver.c
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        size_t required_size = 0;
        // Cek apakah key "ap_ssid" sudah ada di dalam NVS
        esp_err_t err = nvs_get_str(nvs_handle, "ap_ssid", NULL, &required_size);
        
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            // Jika tidak ditemukan (misal setelah Erase Flash), isi dengan default
            ESP_LOGI(TAG, "NVS kosong, menulis default AP: ssid=hydra, pass=notforfun");
            nvs_set_str(nvs_handle, "ap_ssid", "hydra");
            nvs_set_str(nvs_handle, "ap_pass", "notforfun");
            nvs_commit(nvs_handle);
        } else {
            // Jika sudah ada, biarkan saja (artinya user sudah pernah menggantinya lewat Web UI)
            ESP_LOGI(TAG, "Credentials AP kustom ditemukan di NVS, menggunakan pengaturan user.");
        }
        nvs_close(nvs_handle);
    }
    // =========================================================================

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Wi-Fi menyala dan otomatis akan membaca data SSID/Pass dari NVS namespace "storage"
    wifictl_mgmt_ap_start();
    
    attack_init();

    hydra_display_init();

    webserver_run();
}
