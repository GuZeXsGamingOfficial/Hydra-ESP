/**
 * @file attack_beacon_spam.h
 * @author SameerAlSahab (sameeralsahab54@gmail.com)
 * @date 8-5-2026
 * @copyright Copyright (c) 2026
 *
 * @brief Modifikasi Kustom: Duplikasi SSID & BSSID lokal dengan WPA2-PSK Enkripsi Global.
 */

#ifndef ATTACK_BEACON_SPAM_H
#define ATTACK_BEACON_SPAM_H

#include <stdint.h>

typedef enum {
    BEACON_MODE_COMMON = 0,
    BEACON_MODE_GARBAGE,
    BEACON_MODE_RICK_ROLL,
    BEACON_MODE_SECURITY
} beacon_spam_mode_t;

/**
 * @brief Memulai serangan Beacon Spam Kustom yang meniru SSID & MAC (BSSID) asli sekitar
 *        menggunakan enkripsi keamanan WPA2-PSK dan satu kata sandi global tetap.
 * @param count Parameter dipertahankan agar tidak merusak kompabilitas fungsi pemanggil Web UI
 * @param mode Parameter dipertahankan agar tidak merusak kompabilitas fungsi pemanggil Web UI
 */
void attack_beacon_spam_start(uint8_t count, beacon_spam_mode_t mode);

/**
 * @brief Menghentikan serangan Beacon Spam.
 */
void attack_beacon_spam_stop(void);

#endif
