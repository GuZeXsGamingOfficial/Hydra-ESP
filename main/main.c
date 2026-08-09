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
    // LOGIKA PENGECEKAN & PENGISIAN CREDENTIALS DEFAULT AP
    // Default hanya digunakan bila tidak ditemukan data NVS sebelumnya.
    // Jika NVS berisi kredensial AP kustom, jangan timpa.
    // Sinkronisasi mgmt_ssid/mgmt_password ke ap_ssid/ap_pass diperlukan
    // agar wifictl_mgmt_ap_start() selalu menggunakan nilai terbaru.
    // =========================================================================
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        char saved_ssid[33] = {0};
        char saved_pass[65] = {0};
        char mgmt_ssid[33] = {0};
        char mgmt_pass[65] = {0};
        size_t ssid_size = sizeof(saved_ssid);
        size_t pass_size = sizeof(saved_pass);
        size_t mgmt_ssid_size = sizeof(mgmt_ssid);
        size_t mgmt_pass_size = sizeof(mgmt_pass);

        esp_err_t ssid_err = nvs_get_str(nvs_handle, "ap_ssid", saved_ssid, &ssid_size);
        esp_err_t pass_err = nvs_get_str(nvs_handle, "ap_pass", saved_pass, &pass_size);

        bool ap_credentials_missing = false;
        if (ssid_err == ESP_ERR_NVS_NOT_FOUND || ssid_err == ESP_ERR_NVS_INVALID_LENGTH || ssid_size == 0 || strlen(saved_ssid) == 0) {
            ap_credentials_missing = true;
        }
        if (pass_err == ESP_ERR_NVS_NOT_FOUND || pass_err == ESP_ERR_NVS_INVALID_LENGTH || pass_size == 0 || strlen(saved_pass) == 0) {
            ap_credentials_missing = true;
        }

        if (ap_credentials_missing) {
            ESP_LOGI(TAG, "NVS kosong atau korup, menulis default AP: ssid=hydra, pass=notforfun");
            ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ap_ssid", "hydra"));
            ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ap_pass", "notforfun"));
            ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "mgmt_ssid", "hydra"));
            ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "mgmt_password", "notforfun"));
            ESP_ERROR_CHECK(nvs_commit(nvs_handle));
        } else {
            esp_err_t mgmt_ssid_err = nvs_get_str(nvs_handle, "mgmt_ssid", mgmt_ssid, &mgmt_ssid_size);
            esp_err_t mgmt_pass_err = nvs_get_str(nvs_handle, "mgmt_password", mgmt_pass, &mgmt_pass_size);

            bool mgmt_needs_sync = false;
            if (mgmt_ssid_err != ESP_OK || mgmt_ssid_size == 0 || strcmp(saved_ssid, mgmt_ssid) != 0) {
                mgmt_needs_sync = true;
            }
            if (mgmt_pass_err != ESP_OK || mgmt_pass_size == 0 || strcmp(saved_pass, mgmt_pass) != 0) {
                mgmt_needs_sync = true;
            }

            if (mgmt_needs_sync) {
                ESP_LOGI(TAG, "Sinkronisasi kredensial NVS: ap_ssid/ap_pass -> mgmt_ssid/mgmt_password");
                ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "mgmt_ssid", saved_ssid));
                ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "mgmt_password", saved_pass));
                ESP_ERROR_CHECK(nvs_commit(nvs_handle));
            } else {
                ESP_LOGI(TAG, "Kredensial NVS sudah sinkron, menggunakan SSID=%s", saved_ssid);
            }
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
