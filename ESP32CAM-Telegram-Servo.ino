#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <EEPROM.h>
#include <SPIFFS.h>
#include <time.h>

#include "config.h"
#include "definitions.h"
#include "html_page.h"

// ===== Globals =====
WebServer server(80);

int captureMode = 0;
int timeInterval = 5;
bool motionEnabled = true;
int motionThreshold = 5000;

int capturedCount = 0;
int sentCount = 0;

String lastCaptureTime = "Never";
String lastCaptureType = "None";
String lastTelegramResult = "Never";
String telegramDebug = "";

uint8_t* previousFrame = nullptr;
size_t previousFrameSize = 0;
unsigned long lastCaptureMillis = 0;

// persistence throttling
static bool settingsDirty = false;
static unsigned long lastPersistMillis = 0;
static int dirtyCount = 0;

#include "functions.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32-CAM Telegram Servo (Fresh Build) ===");

  EEPROM.begin(EEPROM_SIZE);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    Serial.println("SPIFFS mounted");
  }

  loadSettings();

  if (!initializeCamera()) {
    Serial.println("Camera init failed (still running web+telegram)");
  }

#if SERVO_ENABLED
  servoInit();
#endif

  connectToWiFi();
  setupTimeTehran();

  setupServerRoutes();

  Serial.println("Ready!");
  Serial.println("Web: http://" + WiFi.localIP().toString());

  delay(1200);
  sendTelegramMessage("🚀 ESP32-CAM Started\nIP: " + WiFi.localIP().toString() + "\nTime: " + getTimeString());
}

void loop() {
  server.handleClient();
  checkTelegramCommands();

  persistSettingsIfDue();

  const unsigned long now = millis();

  // time-based capture
  static unsigned long lastTimeCheck = 0;
  if (now - lastTimeCheck >= 1000) {
    lastTimeCheck = now;

    unsigned long intervalSeconds = (unsigned long)timeInterval * 60UL;
    if (intervalSeconds < 60) intervalSeconds = 60;

    unsigned long sec = now / 1000UL;
    if ((captureMode == 1 || captureMode == 2) &&
        (sec % intervalSeconds == 0) &&
        (now - lastCaptureMillis > 30000)) {
      captureImage("Time Based");
    }
  }

  // motion-based capture
  static unsigned long lastMotionCheck = 0;
  if (now - lastMotionCheck >= 500) {
    lastMotionCheck = now;

    if ((captureMode == 0 || captureMode == 2) &&
        detectMotion() &&
        (now - lastCaptureMillis > 12000)) {
      captureImage("Motion");
    }
  }

  delay(5);
}

// persistence helpers (called from functions.h)
void stageSettingsDirty() {
  settingsDirty = true;
  dirtyCount++;
}

void persistSettingsIfDue() {
  const unsigned long now = millis();
  const bool timeDue = (now - lastPersistMillis) > 60000UL;
  const bool countDue = dirtyCount >= 10;

  if (settingsDirty && (timeDue || countDue)) {
    // saveSettings inside functions.h
    extern void saveSettingsNow();
    saveSettingsNow();

    settingsDirty = false;
    dirtyCount = 0;
    lastPersistMillis = now;
  }
}
