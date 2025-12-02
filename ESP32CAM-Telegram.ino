#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <EEPROM.h>
#include "config.h"
#include "definitions.h"  

// Web server
WebServer server(80);

// Capture configuration
int captureMode = 0;
int timeInterval = 5;
bool motionEnabled = true;
int motionThreshold = 5000;

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

// Include HTML page
#include "html_page.h"

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32-CAM Security System ===");
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    loadSettings();

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
    } else {
        Serial.println("SPIFFS mounted successfully");
    }
    
    // Initialize camera
    if (!initializeCamera()) {
        Serial.println("Camera init failed!");
        return;
    }
    
    // Connect to WiFi
    connectToWiFi();
    
    // Setup server
    setupServerRoutes();
    
    Serial.println("=== System Ready ===");
    Serial.println("Web: http://" + WiFi.localIP().toString());
    
    // Send startup message
    delay(2000);
    sendTelegramMessage("🚀 ESP32-CAM Started\nIP: " + WiFi.localIP().toString());
}

void loop() {
    server.handleClient();
    checkTelegramCommands();  

    unsigned long currentMillis = millis();
    static unsigned long lastTimeCheck = 0;
    
    // Time-based capture
    if (currentMillis - lastTimeCheck >= 1000) {
        lastTimeCheck = currentMillis;
        
        unsigned long currentSeconds = currentMillis / 1000;
        unsigned long intervalSeconds = timeInterval * 60;
        
        if ((captureMode == 1 || captureMode == 2) && 
            (currentSeconds % intervalSeconds == 0) && 
            (currentMillis - lastCaptureMillis > 30000)) {
            captureImage("Time Based");
        }
    }
    
    // Motion-based capture
    static unsigned long lastMotionCheck = 0;
    if (currentMillis - lastMotionCheck >= 500) {
        lastMotionCheck = currentMillis;
        
        if ((captureMode == 0 || captureMode == 2) && 
            detectMotion() && 
            (currentMillis - lastCaptureMillis > 10000)) {
            captureImage("Motion Detection");
        }
    }

    delay(10);
}

// Include all function implementations
#include "functions.h"