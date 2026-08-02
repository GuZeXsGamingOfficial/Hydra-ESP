/**
 * @file wifi_controller.h
 * @author risinek (risinek@gmail.com)
 * @date 2021-04-05
 * @copyright Copyright (c) 2021
 * 
 * @brief Provides an interface for common Wi-Fi related operations
 */
#ifndef WIFI_CONTROLLER_H
#define WIFI_CONTROLLER_H

#include <stdint.h>
#include <unistd.h>

#include "../ap_scanner.h"
#include "../sniffer.h"

#include "esp_wifi_types.h"
#include "esp_err.h"

// --- Fungsi Asli Risinek / Hydraproject ---
void wifictl_ap_start(wifi_config_t *wifi_config);
void wifictl_ap_stop();
void wifictl_mgmt_ap_start();
void wifictl_sta_connect_to_ap(const wifi_ap_record_t *ap_record, const char password[]);
void wifictl_sta_disconnect();
void wifictl_set_ap_mac(const uint8_t *mac_ap);
void wifictl_get_ap_mac(uint8_t *mac_ap);
void wifictl_restore_ap_mac();
void wifictl_get_sta_mac(uint8_t *mac_sta);
void wifictl_set_channel(uint8_t channel);
void wifictl_mgmt_ap_stop();

// Read management AP credentials from NVS storage namespace "storage".
// Outputs are written into provided buffers. Returns an esp_err_t.
esp_err_t wifictl_get_mgmt_creds(char *out_ssid, size_t ssid_len, char *out_password, size_t password_len);

// --- Fungsi Tambahan (Vendor Spoofing Dinamis) ---
/**
 * @brief Mengubah OUI MAC Address AP agar menggunakan identitas vendor Huawei
 *        dengan 3 byte terakhir yang diacak secara dinamis via hardware generator.
 */
void wifictl_set_vendor_huawei_random_mac(void);

#endif
