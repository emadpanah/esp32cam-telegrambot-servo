// functions.h - All function implementations
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

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
        config.jpeg_quality = 6;
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
    
    String boundary = "------------------------" + String(millis());
    
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
    
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http.addHeader("Content-Length", String(total_len));
    
    int httpResponseCode = http.POST((uint8_t*)data_part.c_str(), data_part.length());
    
    if (httpResponseCode > 0) {
        Serial.printf("Initial POST: %d\n", httpResponseCode);
        
        WiFiClient *stream = http.getStreamPtr();
        if (stream) {
            size_t written = stream->write(fb->buf, fb->len);
            Serial.printf("Image written: %d/%d bytes\n", written, fb->len);
            
            written = stream->write((uint8_t*)data_tail.c_str(), data_tail.length());
            Serial.printf("Closing boundary written: %d/%d bytes\n", written, data_tail.length());
            
            stream->flush();
            
            httpResponseCode = http.GET();
            String response = http.getString();
            
            Serial.printf("Response code: %d\n", httpResponseCode);
            
            int showLen = response.length();
            if (showLen > 200) showLen = 200;
            Serial.println("Response: " + response.substring(0, showLen));
            
            telegramDebug = "Response: " + String(httpResponseCode);
            
            if (response.indexOf("\"ok\":true") > 0) {
                telegramDebug = "✅ Photo uploaded successfully!";
                http.end();
                return true;
            } else {
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
    
    Serial.println("Trying alternative method...");
    return sendPhotoToTelegramAlternative(fb, caption);
}

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
    
    String requestBody = "--" + boundary + "\r\n";
    requestBody += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
    requestBody += String(TELEGRAM_CHANNEL) + "\r\n";
    
    requestBody += "--" + boundary + "\r\n";
    requestBody += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
    requestBody += caption + "\r\n";
    
    requestBody += "--" + boundary + "\r\n";
    requestBody += "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
    requestBody += "Content-Type: image/jpeg\r\n\r\n";
    
    size_t headerLen = requestBody.length();
    size_t totalLen = headerLen + fb->len + String("\r\n--" + boundary + "--\r\n").length();
    
    uint8_t* fullRequest = (uint8_t*)malloc(totalLen);
    if (!fullRequest) return false;
    
    memcpy(fullRequest, requestBody.c_str(), headerLen);
    memcpy(fullRequest + headerLen, fb->buf, fb->len);
    String footer = "\r\n--" + boundary + "--\r\n";
    memcpy(fullRequest + headerLen + fb->len, footer.c_str(), footer.length());
    
    int httpCode = http.POST(fullRequest, totalLen);
    String response = http.getString();
    
    free(fullRequest);
    http.end();
    
    Serial.printf("Alt method response: %d\n", httpCode);
    
    return (httpCode == 200);
}

void testTelegramConnection() {
    Serial.println("Testing Telegram connection...");
    
    if (sendTelegramMessage("📡 ESP32-CAM Connection Test\n✅ Text messages work!\nIP: " + WiFi.localIP().toString())) {
        Serial.println("Text message sent successfully!");
        telegramDebug = "✅ Text messages work!";
        
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

// ========== TELEGRAM COMMAND HANDLING ==========

// Get the last processed update_id from SPIFFS
long getLastUpdateID() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
        return 0;
    }
    
    if (!SPIFFS.exists(TELEGRAM_OFFSET_FILE)) {
        SPIFFS.end();
        return 0;
    }
    
    File file = SPIFFS.open(TELEGRAM_OFFSET_FILE, "r");
    if (!file) {
        SPIFFS.end();
        return 0;
    }
    
    String content = file.readString();
    file.close();
    SPIFFS.end();
    
    return content.toInt();
}

// Save the last processed update_id to SPIFFS
void saveLastUpdateID(long update_id) {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
        return;
    }
    
    File file = SPIFFS.open(TELEGRAM_OFFSET_FILE, "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        SPIFFS.end();
        return;
    }
    
    file.print(update_id);
    file.close();
    SPIFFS.end();
}

// Parse and clean Telegram command
String parseTelegramCommand(String message) {
    message.trim();
    
    // Remove bot mention if present
    if (message.indexOf('@') > 0) {
        message = message.substring(0, message.indexOf('@'));
    }
    
    return message;
}

// Handle Telegram commands
void handleTelegramCommand(String command) {
    Serial.println("Handling command: " + command);
    
    command.toLowerCase();
    telegramDebug = "Command: " + command;
    
    if (command == "/start" || command == "/help") {
        String help = "🤖 *ESP32-CAM Bot Commands:*\n\n";
        help += "📸 */capture* - Take photo\n";
        help += "📊 */status* - Camera status\n";
        help += "🔍 */test* - Test connection\n";
        help += "⚙️ */settings* - Current settings\n";
        help += "🔄 */reboot* - Restart camera\n";
        help += "❓ */help* - Show this message\n\n";
        help += "📍 *IP:* " + WiFi.localIP().toString() + "\n";
        help += "⏰ *Uptime:* " + getUptimeString();
        
        sendTelegramMessage(help);
    }
    else if (command == "/capture" || command == "/photo" || command == "/pic") {
        sendTelegramMessage("📸 Capturing photo...");
        captureImage("Telegram Command");
    }
    else if (command == "/status" || command == "/info") {
        String status = "📊 *Camera Status:*\n\n";
        status += "📍 *IP:* " + WiFi.localIP().toString() + "\n";
        status += "📡 *WiFi:* " + String(WiFi.RSSI()) + " dBm\n";
        status += "⏰ *Uptime:* " + getUptimeString() + "\n";
        status += "📸 *Captured:* " + String(capturedCount) + "\n";
        status += "📤 *Sent:* " + String(sentCount) + "\n";
        status += "🔄 *Mode:* ";
        status += captureMode == 0 ? "Motion Detection" : 
                 captureMode == 1 ? "Time Based" : "Mixed Mode";
        status += "\n⏱️ *Interval:* " + String(timeInterval) + " min";
        status += "\n🎚️ *Sensitivity:* " + String(motionThreshold);
        
        sendTelegramMessage(status);
    }
    else if (command == "/test") {
        sendTelegramMessage("🔍 Testing connection...");
        testTelegramConnection();
    }
    else if (command == "/settings") {
        String settings = "⚙️ *Current Settings:*\n\n";
        settings += "🔄 *Mode:* ";
        settings += captureMode == 0 ? "Motion Detection" : 
                   captureMode == 1 ? "Time Based" : "Mixed Mode";
        settings += "\n⏱️ *Interval:* " + String(timeInterval) + " min";
        settings += "\n🎚️ *Sensitivity:* " + String(motionThreshold);
        settings += "\n📸 *Last Capture:* " + lastCaptureTime;
        settings += "\n📤 *Last Telegram:* " + lastTelegramResult;
        
        sendTelegramMessage(settings);
    }
    else if (command == "/reboot" || command == "/restart") {
        sendTelegramMessage("🔄 Restarting ESP32-CAM...");
        delay(1000);
        ESP.restart();
    }
    else if (command == "/motion_on") {
        motionEnabled = true;
        saveSettings();
        sendTelegramMessage("✅ Motion detection enabled");
    }
    else if (command == "/motion_off") {
        motionEnabled = false;
        saveSettings();
        sendTelegramMessage("⭕ Motion detection disabled");
    }
    else if (command == "/stream") {
        String streamUrl = "🌐 *Live Stream:*\n";
        streamUrl += "http://" + WiFi.localIP().toString() + "\n";
        streamUrl += "\nOpen in browser to view live feed";
        sendTelegramMessage(streamUrl);
    }
    else {
        sendTelegramMessage("❓ Unknown command: " + command + 
                           "\nType /help for available commands");
    }
}

// Check for new Telegram commands
void checkTelegramCommands() {
    static unsigned long lastCheck = 0;
    unsigned long currentMillis = millis();
    
    // Check every TELEGRAM_POLL_INTERVAL milliseconds
    if (currentMillis - lastCheck >= TELEGRAM_POLL_INTERVAL) {
        lastCheck = currentMillis;
        
        if (WiFi.status() != WL_CONNECTED) {
            return;
        }
        
        long last_update_id = getLastUpdateID();
        
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(5000);
        
        HTTPClient http;
        String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + 
                    "/getUpdates?offset=" + String(last_update_id + 1) + 
                    "&limit=1&timeout=1";
        
        if (http.begin(client, url)) {
            int httpCode = http.GET();
            
            if (httpCode == 200) {
                String response = http.getString();
                
                // Parse JSON response
                StaticJsonDocument<1024> doc;
                DeserializationError error = deserializeJson(doc, response);
                
                if (!error && doc["ok"] == true && doc["result"].size() > 0) {
                    // Get the update ID
                    long update_id = doc["result"][0]["update_id"];
                    
                    // Check if message exists and has text
                    if (doc["result"][0].containsKey("message") && 
                        doc["result"][0]["message"].containsKey("text")) {
                        
                        String messageText = doc["result"][0]["message"]["text"].as<String>();
                        String command = parseTelegramCommand(messageText);
                        
                        // Get sender info for logging
                        String sender = "Unknown";
                        if (doc["result"][0]["message"].containsKey("from")) {
                            if (doc["result"][0]["message"]["from"].containsKey("username")) {
                                sender = doc["result"][0]["message"]["from"]["username"].as<String>();
                            } else if (doc["result"][0]["message"]["from"].containsKey("first_name")) {
                                sender = doc["result"][0]["message"]["from"]["first_name"].as<String>();
                            }
                        }
                        
                        Serial.println("Telegram command from " + sender + ": " + command);
                        telegramDebug = "CMD from " + sender + ": " + command;
                        
                        // Handle the command
                        handleTelegramCommand(command);
                    }
                    
                    // Save the update_id so we don't process it again
                    saveLastUpdateID(update_id);
                }
            } else if (httpCode > 0) {
                Serial.printf("Telegram API error: %d\n", httpCode);
                telegramDebug = "Telegram API error: " + String(httpCode);
            } else {
                Serial.println("Telegram connection failed");
                telegramDebug = "Telegram connection failed";
            }
            
            http.end();
        }
    }
}

#endif