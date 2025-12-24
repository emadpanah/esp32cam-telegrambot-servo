#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <EEPROM.h>
#include <SPIFFS.h>

#include "config.h"
#include "definitions.h"
#include <time.h>


// Web server
WebServer server(80);

// Capture configuration
int captureMode = 0;          // 0 motion, 1 time, 2 mixed
int timeInterval = 5;         // minutes (1..1000)
bool motionEnabled = true;
int motionThreshold = 5000;   // threshold for length-diff heuristic

// Statistics
int capturedCount = 0;
int sentCount = 0;
String lastCaptureTime = "Never";
String lastTelegramResult = "Never";
String lastCaptureType = "None";
String telegramDebug = "";

// Motion detection
uint8_t* previousFrame = nullptr;
size_t previousFrameSize = 0;
unsigned long lastMotionTime = 0;
unsigned long lastCaptureMillis = 0;

// Throttled persistence
static bool statsDirty = false;
static unsigned long lastPersistMillis = 0;
static int dirtySinceLastPersist = 0;

// Include HTML page
#include "html_page.h"

// Forward
static void markStatsDirty();
static void maybePersistStats();

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP32-CAM Security System (Stable Build) ===");

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // SPIFFS mount ONCE
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }

  loadSettings();

  // Camera
  if (!initializeCamera()) {
    Serial.println("Camera init failed!");
    // Keep running so you can still see debug web endpoints and logs
  }

 connectToWiFi();
 setupTimeTehran();

  setupServerRoutes();

  Serial.println("=== System Ready ===");
  Serial.println("Web: http://" + WiFi.localIP().toString());

  delay(1500);
  sendTelegramMessage("🚀 ESP32-CAM Started\nIP: " + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
  checkTelegramCommands();

  unsigned long currentMillis = millis();

  // Persist stats/settings throttled (avoid flash wear & stalls)
  maybePersistStats();

  // Time-based capture check (each second)
  static unsigned long lastTimeCheck = 0;
  if (currentMillis - lastTimeCheck >= 1000) {
    lastTimeCheck = currentMillis;

    unsigned long intervalSeconds = (unsigned long)timeInterval * 60UL;
    if (intervalSeconds < 60) intervalSeconds = 60;

    unsigned long currentSeconds = currentMillis / 1000UL;

    if ((captureMode == 1 || captureMode == 2) &&
        (currentSeconds % intervalSeconds == 0) &&
        (currentMillis - lastCaptureMillis > 30000)) {
      captureImage("Time Based");
    }
  }

  // Motion-based capture check (each 500ms)
  static unsigned long lastMotionCheck = 0;
  if (currentMillis - lastMotionCheck >= 500) {
    lastMotionCheck = currentMillis;

    if ((captureMode == 0 || captureMode == 2) &&
        detectMotion() &&
        (currentMillis - lastCaptureMillis > 10000)) {
      captureImage("Motion Detection");
    }
  }

  delay(5);
}

static void setupTimeTehran() {
  // Tehran = UTC+3:30 (no DST)
  // POSIX TZ: "IRST-3:30" means UTC+3:30
  const char* TZ_TEHRAN = "IRST-3:30";

  // NTP servers
  configTzTime(TZ_TEHRAN, "pool.ntp.org", "time.google.com", "time.windows.com");

  // wait until time is synced (up to ~10s)
  struct tm timeinfo;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&timeinfo, 500)) {
      Serial.printf("Time synced (Tehran): %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      return;
    }
    delay(500);
  }

  Serial.println("Time sync failed (Tehran) - will retry implicitly later.");
}


static void markStatsDirty() {
  statsDirty = true;
  dirtySinceLastPersist++;
}

static void maybePersistStats() {
  const unsigned long now = millis();
  const bool timeDue = (now - lastPersistMillis) > 60000UL;   // 60s
  const bool countDue = dirtySinceLastPersist >= 10;          // or 10 changes

  if (statsDirty && (timeDue || countDue)) {
    saveSettings(); // writes struct + commit
    statsDirty = false;
    dirtySinceLastPersist = 0;
    lastPersistMillis = now;
  }
}

// Include all function implementations
#include "functions.h"
