// definitions.h - Function declarations
#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <EEPROM.h>
#include <SPIFFS.h>

// Global variables (extern declarations)
extern WebServer server;
extern int captureMode;
extern int timeInterval;
extern bool motionEnabled;
extern int motionThreshold;
extern int capturedCount;
extern int sentCount;
extern String lastCaptureTime;
extern String lastTelegramResult;
extern String lastCaptureType;
extern String telegramDebug;
extern uint8_t* previousFrame;
extern size_t previousFrameSize;
extern unsigned long lastMotionTime;
extern unsigned long lastCaptureMillis;

// Function declarations
bool initializeCamera();
void loadSettings();
void saveSettings();
void connectToWiFi();
void setupServerRoutes();
bool detectMotion();
void captureImage(String type);
bool sendTelegramMessage(String message);
bool sendPhotoToTelegram(camera_fb_t *fb, String caption);
bool sendPhotoToTelegramAlternative(camera_fb_t *fb, String caption);
void testTelegramConnection();
String getTimeString();
String getUptimeString();

// definitions.h - Add these function declarations at the end
void checkTelegramCommands();
void handleTelegramCommand(String command);
String parseTelegramCommand(String message);
long getLastUpdateID();
void saveLastUpdateID(long update_id);

#endif