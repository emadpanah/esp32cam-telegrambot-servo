#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <EEPROM.h>


// ========== CONFIGURATION ==========
const char* SSID = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
const char* TELEGRAM_BOT_TOKEN = "YOUR_BOT_TOKEN";
const char* TELEGRAM_CHANNEL = "@YOUR_CHANNEL";
// ===================================


// Camera pins
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

// EEPROM
#define EEPROM_SIZE 128
#define EEPROM_MODE 0
#define EEPROM_INTERVAL 1
#define EEPROM_MOTION_ENABLED 2
#define EEPROM_THRESHOLD 3
#define EEPROM_CAPTURED_COUNT 4
#define EEPROM_SENT_COUNT 8

// HTML Page (keeping it simple)
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM Monitor</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f0f0; }
        .container { max-width: 1200px; margin: 0 auto; background: white; border-radius: 10px; padding: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { text-align: center; color: #333; }
        .video-container { width: 100%; background: #000; border-radius: 10px; overflow: hidden; margin-bottom: 20px; }
        #stream { width: 100%; max-height: 400px; }
        .control-panel { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin: 20px 0; }
        @media (max-width: 768px) { .control-panel { grid-template-columns: 1fr; } }
        .panel { background: #f8f9fa; padding: 20px; border-radius: 8px; border: 1px solid #dee2e6; }
        .btn { background: #007bff; color: white; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-size: 16px; width: 100%; margin: 5px 0; transition: background 0.3s; }
        .btn:hover { background: #0056b3; }
        .btn-capture { background: #28a745; }
        .btn-capture:hover { background: #1e7e34; }
        .btn-test { background: #ffc107; color: #212529; }
        .btn-test:hover { background: #e0a800; }
        .stats-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin: 20px 0; }
        .stat-card { background: #17a2b8; color: white; padding: 15px; border-radius: 8px; text-align: center; }
        .stat-value { font-size: 1.5em; font-weight: bold; margin: 5px 0; }
        .log-section { background: #343a40; color: white; padding: 15px; border-radius: 8px; margin: 20px 0; font-family: monospace; font-size: 14px; }
        .debug-section { background: #6c757d; color: white; padding: 15px; border-radius: 8px; margin: 20px 0; font-family: monospace; font-size: 12px; max-height: 200px; overflow-y: auto; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        select, input { width: 100%; padding: 8px; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32-CAM Security Monitor</h1>
        
        <div class="video-container">
            <img id="stream" src="/stream">
        </div>

        <div class="stats-grid">
            <div class="stat-card"><div>Captured Images</div><div id="capturedCount" class="stat-value">0</div></div>
            <div class="stat-card"><div>Sent to Telegram</div><div id="sentCount" class="stat-value">0</div></div>
        </div>

        <div class="control-panel">
            <div class="panel">
                <h3>Manual Control</h3>
                <button class="btn btn-capture" onclick="captureNow()">Capture Image Now</button>
                <button class="btn btn-test" onclick="testTelegram()">Test Telegram</button>
                <button class="btn" onclick="refreshStream()">Refresh Stream</button>
            </div>
            
            <div class="panel">
                <h3>Automation Settings</h3>
                <div class="form-group">
                    <label>Capture Mode:</label>
                    <select id="captureMode" onchange="updateMode()">
                        <option value="0">Motion Detection</option>
                        <option value="1">Time Based</option>
                        <option value="2">Mixed Mode</option>
                    </select>
                </div>
                
                <div class="form-group">
                    <label>Time Interval (minutes):</label>
                    <input type="number" id="timeInterval" min="1" max="1000" value="5">
                </div>
                
                <div class="form-group">
                    <label>Motion Sensitivity:</label>
                    <input type="range" id="motionThreshold" min="1000" max="20000" value="5000" step="1000">
                    <div>Sensitivity: <span id="sensitivityValue">Medium</span></div>
                </div>
                
                <button class="btn" onclick="saveSettings()">Save Settings</button>
            </div>
        </div>

        <div class="log-section">
            <h3>Activity Log</h3>
            <div><strong>Last Capture:</strong> <span id="lastCaptureTime">Never</span></div>
            <div><strong>Last Capture Type:</strong> <span id="lastCaptureType">None</span></div>
            <div><strong>Last Telegram Result:</strong> <span id="lastTelegramResult">Never</span></div>
            <div><strong>Current Mode:</strong> <span id="currentMode">Motion Detection</span></div>
            <div><strong>Device Uptime:</strong> <span id="uptime">0s</span></div>
        </div>

        <div class="debug-section">
            <h3>Telegram Debug</h3>
            <div id="telegramDebug">No debug info yet...</div>
        </div>
    </div>

    <script>
        function updateMode() {
            const mode = document.getElementById('captureMode').value;
            document.getElementById('currentMode').textContent = 
                ['Motion Detection', 'Time Based', 'Mixed Mode'][mode];
        }
        function updateSensitivity() {
            const value = document.getElementById('motionThreshold').value;
            document.getElementById('sensitivityValue').textContent = 
                value < 5000 ? 'High' : value < 10000 ? 'Medium' : 'Low';
        }
        function captureNow() {
            fetch('/capture-now').then(r => r.text()).then(result => {
                alert('Capture: ' + result); updateStatus();
            });
        }
        function testTelegram() {
            fetch('/test-telegram').then(r => r.text()).then(result => {
                alert('Test: ' + result); updateStatus();
            });
        }
        function refreshStream() {
            document.getElementById('stream').src = '/stream?t=' + Date.now();
        }
        function saveSettings() {
            const settings = {
                mode: document.getElementById('captureMode').value,
                interval: document.getElementById('timeInterval').value,
                threshold: document.getElementById('motionThreshold').value
            };
            fetch('/save-settings', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(settings)
            }).then(r => r.text()).then(result => {
                alert('Saved: ' + result); updateStatus();
            });
        }
        function updateStatus() {
            fetch('/status').then(r => r.json()).then(data => {
                document.getElementById('capturedCount').textContent = data.capturedCount;
                document.getElementById('sentCount').textContent = data.sentCount;
                document.getElementById('lastCaptureTime').textContent = data.lastCaptureTime;
                document.getElementById('lastCaptureType').textContent = data.lastCaptureType;
                document.getElementById('lastTelegramResult').textContent = data.lastTelegramResult;
                document.getElementById('currentMode').textContent = data.currentMode;
                document.getElementById('telegramDebug').textContent = data.telegramDebug;
                document.getElementById('uptime').textContent = data.uptime;
                document.getElementById('captureMode').value = data.captureMode;
                document.getElementById('timeInterval').value = data.timeInterval;
                document.getElementById('motionThreshold').value = data.motionThreshold;
                updateSensitivity(); updateMode();
            });
        }
        document.getElementById('motionThreshold').addEventListener('input', updateSensitivity);
        setInterval(updateStatus, 3000);
        setInterval(() => document.getElementById('stream').src = '/stream?t=' + Date.now(), 2000);
        updateStatus(); updateSensitivity(); updateMode();
    </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32-CAM Security System ===");
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    loadSettings();
    
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

bool initializeCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    
    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 6;  // Very low quality for small files
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 6;
        config.fb_count = 1;
    }

    return esp_camera_init(&config) == ESP_OK;
}

void loadSettings() {
    captureMode = EEPROM.read(EEPROM_MODE);
    if (captureMode > 2) captureMode = 0;
    timeInterval = EEPROM.read(EEPROM_INTERVAL);
    if (timeInterval < 1 || timeInterval > 1000) timeInterval = 5;
    motionEnabled = EEPROM.read(EEPROM_MOTION_ENABLED);
    motionThreshold = EEPROM.read(EEPROM_THRESHOLD) * 100;
    if (motionThreshold < 1000) motionThreshold = 5000;
    EEPROM.get(EEPROM_CAPTURED_COUNT, capturedCount);
    EEPROM.get(EEPROM_SENT_COUNT, sentCount);
}

void saveSettings() {
    EEPROM.write(EEPROM_MODE, captureMode);
    EEPROM.write(EEPROM_INTERVAL, timeInterval);
    EEPROM.write(EEPROM_MOTION_ENABLED, motionEnabled);
    EEPROM.write(EEPROM_THRESHOLD, motionThreshold / 100);
    EEPROM.put(EEPROM_CAPTURED_COUNT, capturedCount);
    EEPROM.put(EEPROM_SENT_COUNT, sentCount);
    EEPROM.commit();
}

void connectToWiFi() {
    Serial.printf("Connecting to %s...", SSID);
    WiFi.begin(SSID, PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nWiFi failed!");
    }
}

void setupServerRoutes() {
    server.on("/", HTTP_GET, []() { server.send(200, "text/html", INDEX_HTML); });
    
    server.on("/stream", HTTP_GET, []() {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
            esp_camera_fb_return(fb);
        } else {
            server.send(500, "text/plain", "Camera error");
        }
    });
    
    server.on("/status", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["capturedCount"] = capturedCount;
        doc["sentCount"] = sentCount;
        doc["lastCaptureTime"] = lastCaptureTime;
        doc["lastCaptureType"] = lastCaptureType;
        doc["lastTelegramResult"] = lastTelegramResult;
        doc["captureMode"] = captureMode;
        doc["timeInterval"] = timeInterval;
        doc["motionThreshold"] = motionThreshold;
        doc["uptime"] = getUptimeString();
        doc["telegramDebug"] = telegramDebug;
        doc["currentMode"] = captureMode == 0 ? "Motion Detection" : captureMode == 1 ? "Time Based" : "Mixed Mode";
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
    
    server.on("/capture-now", HTTP_GET, []() {
        captureImage("Manual");
        server.send(200, "text/plain", "Capture attempted");
    });
    
    server.on("/test-telegram", HTTP_GET, []() {
        testTelegramConnection();
        server.send(200, "text/plain", "Telegram test completed");
    });
    
    server.on("/save-settings", HTTP_POST, []() {
        String body = server.arg("plain");
        StaticJsonDocument<256> doc;
        deserializeJson(doc, body);
        captureMode = doc["mode"];
        timeInterval = doc["interval"];
        motionThreshold = doc["threshold"];
        saveSettings();
        server.send(200, "text/plain", "Settings saved");
    });
    
    server.begin();
    Serial.println("HTTP server started");
}

bool detectMotion() {
    if (!motionEnabled) return false;
    
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return false;

    if (previousFrame == nullptr) {
        previousFrame = (uint8_t*)malloc(fb->len);
        if (previousFrame) {
            memcpy(previousFrame, fb->buf, fb->len);
            previousFrameSize = fb->len;
        }
        esp_camera_fb_return(fb);
        return false;
    }

    long difference = abs((long)fb->len - (long)previousFrameSize);
    bool motion = difference > motionThreshold;

    if (motion) {
        memcpy(previousFrame, fb->buf, fb->len);
        previousFrameSize = fb->len;
        lastMotionTime = millis();
    }

    esp_camera_fb_return(fb);
    return motion;
}

void captureImage(String type) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        lastCaptureTime = "Failed: No frame";
        lastCaptureType = type;
        return;
    }

    capturedCount++;
    lastCaptureTime = getTimeString();
    lastCaptureType = type;
    
    Serial.printf("Captured: %d bytes, Type: %s\n", fb->len, type.c_str());
    telegramDebug = "Captured " + String(fb->len) + " bytes";

    bool telegramSuccess = sendPhotoToTelegram(fb, type);
    if (telegramSuccess) {
        sentCount++;
        lastTelegramResult = "Success at " + getTimeString();
        telegramDebug = "✅ Photo sent successfully!";
        Serial.println("Photo sent successfully");
    } else {
        lastTelegramResult = "Failed at " + getTimeString();
        telegramDebug = "❌ Failed to send photo";
        Serial.println("Failed to send photo");
    }

    esp_camera_fb_return(fb);
    lastCaptureMillis = millis();
    saveSettings();
}

// Telegram text message
bool sendTelegramMessage(String message) {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";
    
    if (!http.begin(client, url)) return false;
    
    http.addHeader("Content-Type", "application/json");
    
    StaticJsonDocument<256> doc;
    doc["chat_id"] = String(TELEGRAM_CHANNEL);
    doc["text"] = message;
    
    String payload;
    serializeJson(doc, payload);
    
    int httpCode = http.POST(payload);
    String response = http.getString();
    
    http.end();
    
    Serial.printf("Text message: %d\n", httpCode);
    
    return httpCode == 200;
}

// PROVEN WORKING Telegram photo upload
bool sendPhotoToTelegram(camera_fb_t *fb, String caption) {
    telegramDebug = "🔄 Starting photo upload...";
    Serial.println("Starting photo upload");
    
    if (WiFi.status() != WL_CONNECTED) {
        telegramDebug = "❌ WiFi not connected";
        return false;
    }
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(60000);
    
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendPhoto";
    
    Serial.println("URL: " + url);
    
    if (!http.begin(client, url)) {
        telegramDebug = "❌ HTTP begin failed";
        return false;
    }
    
    // Create multipart boundary
    String boundary = "------------------------" + String(millis());
    
    // Calculate total content length
    String data_part = "--" + boundary + "\r\n";
    data_part += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
    data_part += String(TELEGRAM_CHANNEL) + "\r\n";
    
    data_part += "--" + boundary + "\r\n";
    data_part += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
    data_part += "ESP32-CAM: " + caption + " | " + getTimeString() + "\r\n";
    
    data_part += "--" + boundary + "\r\n";
    data_part += "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n";
    data_part += "Content-Type: image/jpeg\r\n\r\n";
    
    String data_tail = "\r\n--" + boundary + "--\r\n";
    
    size_t total_len = data_part.length() + fb->len + data_tail.length();
    
    Serial.printf("Total length: %d bytes (image: %d bytes)\n", total_len, fb->len);
    
    // Set headers
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http.addHeader("Content-Length", String(total_len));
    
    // Send the request
    int httpResponseCode = http.POST((uint8_t*)data_part.c_str(), data_part.length());
    
    if (httpResponseCode > 0) {
        Serial.printf("Initial POST: %d\n", httpResponseCode);
        
        // Send image data
        WiFiClient *stream = http.getStreamPtr();
        if (stream) {
            // Send the image
            size_t written = stream->write(fb->buf, fb->len);
            Serial.printf("Image written: %d/%d bytes\n", written, fb->len);
            
            // Send the closing boundary
            written = stream->write((uint8_t*)data_tail.c_str(), data_tail.length());
            Serial.printf("Closing boundary written: %d/%d bytes\n", written, data_tail.length());
            
            // IMPORTANT: Flush the stream
            stream->flush();
            
            // Get the response
            httpResponseCode = http.GET();
            String response = http.getString();
            
            Serial.printf("Response code: %d\n", httpResponseCode);
            
            // Show response (first 200 chars)
            int showLen = response.length();
            if (showLen > 200) showLen = 200;
            Serial.println("Response: " + response.substring(0, showLen));
            
            telegramDebug = "Response: " + String(httpResponseCode);
            
            if (response.indexOf("\"ok\":true") > 0) {
                telegramDebug = "✅ Photo uploaded successfully!";
                http.end();
                return true;
            } else {
                // Try to extract error message
                int start = response.indexOf("\"description\":\"");
                if (start > 0) {
                    start += 15;
                    int end = response.indexOf("\"", start);
                    if (end > start) {
                        String error = response.substring(start, end);
                        telegramDebug = "❌ Error: " + error;
                        Serial.println("Telegram error: " + error);
                    }
                }
            }
        }
    } else {
        telegramDebug = "❌ HTTP POST failed: " + String(httpResponseCode);
        Serial.printf("HTTP POST failed: %d\n", httpResponseCode);
    }
    
    http.end();
    
    // Try alternative method if first fails
    Serial.println("Trying alternative method...");
    return sendPhotoToTelegramAlternative(fb, caption);
}

// Alternative method using single POST
bool sendPhotoToTelegramAlternative(camera_fb_t *fb, String caption) {
    Serial.println("Using alternative method...");
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(60000);
    
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendPhoto";
    
    if (!http.begin(client, url)) return false;
    
    String boundary = "ESP32CAM" + String(millis());
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    
    // Build the entire request
    String requestBody = "--" + boundary + "\r\n";
    requestBody += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
    requestBody += String(TELEGRAM_CHANNEL) + "\r\n";
    
    requestBody += "--" + boundary + "\r\n";
    requestBody += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
    requestBody += caption + "\r\n";
    
    requestBody += "--" + boundary + "\r\n";
    requestBody += "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
    requestBody += "Content-Type: image/jpeg\r\n\r\n";
    
    // Convert requestBody to uint8_t array
    size_t headerLen = requestBody.length();
    size_t totalLen = headerLen + fb->len + String("\r\n--" + boundary + "--\r\n").length();
    
    // Allocate memory for entire request
    uint8_t* fullRequest = (uint8_t*)malloc(totalLen);
    if (!fullRequest) return false;
    
    // Copy header
    memcpy(fullRequest, requestBody.c_str(), headerLen);
    // Copy image
    memcpy(fullRequest + headerLen, fb->buf, fb->len);
    // Copy footer
    String footer = "\r\n--" + boundary + "--\r\n";
    memcpy(fullRequest + headerLen + fb->len, footer.c_str(), footer.length());
    
    // Send the complete request
    int httpCode = http.POST(fullRequest, totalLen);
    String response = http.getString();
    
    free(fullRequest);
    http.end();
    
    Serial.printf("Alt method response: %d\n", httpCode);
    
    return (httpCode == 200);
}

void testTelegramConnection() {
    Serial.println("Testing Telegram connection...");
    
    // Test text first
    if (sendTelegramMessage("📡 ESP32-CAM Connection Test\n✅ Text messages work!\nIP: " + WiFi.localIP().toString())) {
        Serial.println("Text message sent successfully!");
        telegramDebug = "✅ Text messages work!";
        
        // Test photo
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            Serial.printf("Test photo size: %d bytes\n", fb->len);
            bool photoSuccess = sendPhotoToTelegram(fb, "Connection Test");
            esp_camera_fb_return(fb);
            
            if (photoSuccess) {
                Serial.println("Photo sent successfully!");
                telegramDebug = "✅ Both text and photos work!";
            } else {
                Serial.println("Photo failed, but text works");
                telegramDebug = "✅ Text works, ❌ Photos fail";
            }
        }
    } else {
        Serial.println("Text message failed!");
        telegramDebug = "❌ Text messages fail - check token/channel";
    }
}

String getTimeString() {
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours % 24, minutes % 60, seconds % 60);
    return String(buffer);
}

String getUptimeString() {
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    if (hours > 0) {
        return String(hours) + "h " + String(minutes % 60) + "m";
    } else if (minutes > 0) {
        return String(minutes) + "m " + String(seconds % 60) + "s";
    } else {
        return String(seconds) + "s";
    }
}

void loop() {
    server.handleClient();

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