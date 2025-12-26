#ifndef CONFIG_H
#define CONFIG_H

// ===== WiFi =====
const char* SSID = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";

// ===== Telegram =====
const char* TELEGRAM_BOT_TOKEN = "YOUR_BOT_TOKEN";
const char* TELEGRAM_CHAT_ID   = "-1001234567890"; // channel/group id (starts with -100...) OR user id

// ===== Camera pins (AI Thinker) =====
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

// ===== Telegram polling =====
#define TELEGRAM_POLL_INTERVAL 2000
#define TELEGRAM_OFFSET_FILE "/tg_offset.txt"

// ===== EEPROM =====
#define EEPROM_SIZE 128

// ===== Servo (Pan/Tilt) =====
#define SERVO_ENABLED 1

#define SERVO_PAN_PIN   14
#define SERVO_TILT_PIN  -1   // set -1 if you only have one servo

#define PAN_MIN     10
#define PAN_MAX     170
#define TILT_MIN    20
#define TILT_MAX    160

#define PAN_CENTER  90
#define TILT_CENTER 90

#define SERVO_STEP   5

#define FULLSEC_STEP_DEG   10
#define FULLSEC_SETTLE_MS  350
#define FULLSEC_COOLDOWN_MS 1200


#endif
