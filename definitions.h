#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <WebServer.h>
#include "esp_camera.h"

// Globals
extern WebServer server;

extern int captureMode;          // 0 motion, 1 time, 2 mixed
extern int timeInterval;         // minutes
extern bool motionEnabled;
extern int motionThreshold;

extern int capturedCount;
extern int sentCount;

extern String lastCaptureTime;
extern String lastCaptureType;
extern String lastTelegramResult;
extern String telegramDebug;

extern uint8_t* previousFrame;
extern size_t previousFrameSize;
extern unsigned long lastCaptureMillis;

// Functions
bool initializeCamera();
void connectToWiFi();
void setupTimeTehran();
void setupServerRoutes();

bool detectMotion();
void captureImage(const String& type);

bool sendTelegramMessage(const String& text);
bool sendPhotoToTelegram(camera_fb_t* fb, const String& caption);

void testTelegramConnection();

String getTimeString();
String getUptimeString();

void loadSettings();
void stageSettingsDirty();
void persistSettingsIfDue();

// Telegram commands
void checkTelegramCommands();
String parseTelegramCommand(String s);
void handleTelegramCommand(String cmd);

// Servo
void servoInit();
void servoCenter(bool smooth = true);

#endif
