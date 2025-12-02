// config.example.h - Configuration template
#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

// ========== WIFI CONFIGURATION ==========
const char* SSID = "YOUR_WIFI_SSID_HERE";
const char* PASSWORD = "YOUR_WIFI_PASSWORD_HERE";

// ========== TELEGRAM CONFIGURATION ==========
const char* TELEGRAM_BOT_TOKEN = "YOUR_BOT_TOKEN_HERE";
const char* TELEGRAM_CHANNEL = "@YOUR_CHANNEL_HERE";

// ========== CAMERA PINS ==========
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ========== EEPROM ADDRESSES ==========
#define EEPROM_SIZE 128
#define EEPROM_MODE 0
#define EEPROM_INTERVAL 1
#define EEPROM_MOTION_ENABLED 2
#define EEPROM_THRESHOLD 3
#define EEPROM_CAPTURED_COUNT 4
#define EEPROM_SENT_COUNT 8

#endif