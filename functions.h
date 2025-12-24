#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "esp_system.h"   // ✅ for esp_reset_reason()

// ------------ Persisted struct (EEPROM) ------------
static const uint8_t PERSIST_MAGIC = 0xA7;

struct Persisted {
  uint8_t magic;
  uint8_t mode;            // 0..2
  uint16_t intervalMin;    // 1..1000
  uint8_t motionEn;        // 0/1
  uint16_t threshold;      // 1000..20000
  uint32_t captured;
  uint32_t sent;
  uint32_t reserved;       // future
};

// Forward from main for throttling
extern void (*__dummy_throttling_hook)(); // not used, just to avoid warnings

// ------------ Utils ------------
static String resetReasonString() {
  esp_reset_reason_t r = esp_reset_reason();
  switch (r) {
    case ESP_RST_UNKNOWN:    return "UNKNOWN";
    case ESP_RST_POWERON:    return "POWERON";
    case ESP_RST_EXT:        return "EXT_RESET";
    case ESP_RST_SW:         return "SW_RESET";
    case ESP_RST_PANIC:      return "PANIC";
    case ESP_RST_INT_WDT:    return "INT_WDT";
    case ESP_RST_TASK_WDT:   return "TASK_WDT";
    case ESP_RST_WDT:        return "WDT";
    case ESP_RST_DEEPSLEEP:  return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:   return "BROWNOUT";
    case ESP_RST_SDIO:       return "SDIO";
    default:                 return "OTHER";
  }
}

static int resetReasonCode() {
  return (int)esp_reset_reason();
}

static void printMemStats(const char* tag) {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minHeap  = ESP.getMinFreeHeap();
  uint32_t freePs   = 0;
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM_SUPPORT)
  freePs = ESP.getFreePsram();
#endif
  Serial.printf("[%s] freeHeap=%u  minHeap=%u  freePSRAM=%u  reset=%s(%d)\n",
                tag, freeHeap, minHeap, freePs,
                resetReasonString().c_str(), resetReasonCode());
}

// ------------ Camera ------------
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

  // FIX: If PSRAM exists → allow larger frames / 2 frame buffers
  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;   // stable on PSRAM boards
    config.jpeg_quality = 10;            // 6 can be heavy; 10 is OK
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;    // smaller when no PSRAM
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("esp_camera_init failed: 0x%x\n", (int)err);
    return false;
  }

  Serial.println("Camera initialized");
  printMemStats("cam_init");
  return true;
}

// ------------ Settings (EEPROM) ------------
void loadSettings() {
  Persisted p;
  EEPROM.get(0, p);

  if (p.magic != PERSIST_MAGIC) {
    // Defaults
    captureMode = 0;
    timeInterval = 5;
    motionEnabled = true;
    motionThreshold = 5000;
    capturedCount = 0;
    sentCount = 0;
    Serial.println("EEPROM: no valid data, using defaults");
    return;
  }

  captureMode = (p.mode <= 2) ? p.mode : 0;

  int ti = (int)p.intervalMin;
  if (ti < 1 || ti > 1000) ti = 5;
  timeInterval = ti;

  motionEnabled = (p.motionEn != 0);

  int th = (int)p.threshold;
  if (th < 1000 || th > 20000) th = 5000;
  motionThreshold = th;

  capturedCount = (int)p.captured;
  sentCount = (int)p.sent;

  Serial.println("EEPROM settings loaded");
}

void saveSettings() {
  Persisted p;
  p.magic = PERSIST_MAGIC;
  p.mode = (uint8_t)captureMode;
  p.intervalMin = (uint16_t)timeInterval;
  p.motionEn = (uint8_t)(motionEnabled ? 1 : 0);
  p.threshold = (uint16_t)motionThreshold;
  p.captured = (uint32_t)capturedCount;
  p.sent = (uint32_t)sentCount;
  p.reserved = 0;

  EEPROM.put(0, p);
  EEPROM.commit();
  Serial.println("EEPROM committed (throttled)");
}

// ------------ WiFi ------------

#include <time.h>

void connectToWiFi() {
  Serial.printf("Connecting to %s...", SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed!");
  }

  printMemStats("wifi");
}

// ------------ Web routes ------------
void setupServerRoutes() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", INDEX_HTML);
  });

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
    StaticJsonDocument<768> doc;
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
    doc["currentMode"] = (captureMode == 0) ? "Motion Detection" :
                         (captureMode == 1) ? "Time Based" : "Mixed Mode";

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  server.on("/debug", HTTP_GET, []() {
    StaticJsonDocument<768> doc;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["minFreeHeap"] = ESP.getMinFreeHeap();
    doc["resetReason"] = resetReasonString();
    doc["resetReasonCode"] = resetReasonCode();
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM_SUPPORT)
    doc["freePsram"] = ESP.getFreePsram();
#else
    doc["freePsram"] = 0;
#endif
    doc["wifiRSSI"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    doc["uptime"] = getUptimeString();

    String response;
    serializeJsonPretty(doc, response);
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
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      server.send(400, "text/plain", "Bad JSON");
      return;
    }
    captureMode = (int)doc["mode"];
    if (captureMode < 0 || captureMode > 2) captureMode = 0;

    timeInterval = (int)doc["interval"];
    if (timeInterval < 1) timeInterval = 1;
    if (timeInterval > 1000) timeInterval = 1000;

    motionThreshold = (int)doc["threshold"];
    if (motionThreshold < 1000) motionThreshold = 1000;
    if (motionThreshold > 20000) motionThreshold = 20000;

    // Mark dirty (throttled commit)
    extern void markStatsDirty(); // from .ino
    markStatsDirty();

    server.send(200, "text/plain", "Settings staged (will persist soon)");
  });

  server.begin();
  Serial.println("HTTP server started");
}

// ------------ Motion detection (safe) ------------
bool detectMotion() {
  if (!motionEnabled) return false;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;

  // First frame init
  if (previousFrame == nullptr) {
    previousFrame = (uint8_t*)malloc(fb->len);
    if (previousFrame) {
      memcpy(previousFrame, fb->buf, fb->len);
      previousFrameSize = fb->len;
    }
    esp_camera_fb_return(fb);
    return false;
  }

  // Heuristic: compare jpeg size changes (simple, fast)
  long diff = labs((long)fb->len - (long)previousFrameSize);
  bool motion = (diff > motionThreshold);

  // ensure buffer can hold new size before memcpy
  if (motion) {
    if (fb->len > previousFrameSize) {
      uint8_t* p = (uint8_t*)realloc(previousFrame, fb->len);
      if (!p) {
        esp_camera_fb_return(fb);
        return false;
      }
      previousFrame = p;
      previousFrameSize = fb->len;
    }
    memcpy(previousFrame, fb->buf, fb->len);
    previousFrameSize = fb->len;
    lastMotionTime = millis();
  }

  esp_camera_fb_return(fb);
  return motion;
}

// ------------ Capture ------------
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

  Serial.printf("Captured: %u bytes, Type: %s\n", (unsigned)fb->len, type.c_str());
  telegramDebug = "Captured " + String((unsigned)fb->len) + " bytes";

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

  // Throttle persistence
  extern void markStatsDirty(); // from .ino
  markStatsDirty();

  printMemStats("after_capture");
}

// ------------ Telegram: text ------------
bool sendTelegramMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";

  if (!http.begin(client, url)) return false;

  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;
  doc["chat_id"] = String(TELEGRAM_CHANNEL);
  doc["text"] = message;

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  http.end();

  Serial.printf("Text message http=%d\n", httpCode);
  return httpCode == 200;
}

// ------------ Telegram: photo (STREAMING, no big malloc) ------------
static bool readHttpResponse(WiFiClientSecure& client, String& out) {
  unsigned long start = millis();
  out = "";

  while (client.connected() && (millis() - start) < 15000UL) {
    while (client.available()) {
      char c = (char)client.read();
      out += c;
      if (out.length() > 3000) {
        out.remove(0, 1500);
      }
    }
    if (!client.available()) delay(10);
  }
  return out.length() > 0;
}

bool sendPhotoToTelegram(camera_fb_t *fb, String caption) {
  telegramDebug = "🔄 Upload (streaming)...";

  if (WiFi.status() != WL_CONNECTED) {
    telegramDebug = "❌ WiFi not connected";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60000);

  const char* host = "api.telegram.org";
  if (!client.connect(host, 443)) {
    telegramDebug = "❌ TLS connect failed";
    return false;
  }

  String boundary = "----ESP32CAM" + String(millis());

  String part1 =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
    String(TELEGRAM_CHANNEL) + "\r\n";

  String part2 =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"caption\"\r\n\r\n"
    "ESP32-CAM: " + caption + " | " + getTimeString() + "\r\n";

  String part3 =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  size_t contentLength = part1.length() + part2.length() + part3.length() + fb->len + tail.length();

  String req =
    "POST /bot" + String(TELEGRAM_BOT_TOKEN) + "/sendPhoto HTTP/1.1\r\n"
    "Host: " + String(host) + "\r\n"
    "User-Agent: ESP32CAM\r\n"
    "Connection: close\r\n"
    "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
    "Content-Length: " + String((unsigned)contentLength) + "\r\n\r\n";

  client.print(req);
  client.print(part1);
  client.print(part2);
  client.print(part3);

  const uint8_t* p = fb->buf;
  size_t left = fb->len;
  const size_t CHUNK = 1024;

  while (left > 0) {
    size_t n = (left > CHUNK) ? CHUNK : left;
    size_t w = client.write(p, n);
    if (w == 0) {
      telegramDebug = "❌ write failed";
      client.stop();
      return false;
    }
    p += w;
    left -= w;
    delay(0);
  }

  client.print(tail);

  String response;
  readHttpResponse(client, response);
  client.stop();

  bool ok200 = response.indexOf(" 200 ") >= 0;
  bool okJson = response.indexOf("\"ok\":true") >= 0;

  if (ok200 && okJson) {
    telegramDebug = "✅ Photo uploaded!";
    return true;
  }

  telegramDebug = "❌ Upload failed";
  Serial.println("Telegram response (trimmed):");
  Serial.println(response);
  return false;
}

// Kept for compatibility (not used anymore)
bool sendPhotoToTelegramAlternative(camera_fb_t *fb, String caption) {
  (void)fb; (void)caption;
  return false;
}

// ------------ Telegram test ------------
void testTelegramConnection() {
  Serial.println("Testing Telegram connection...");

  bool textOK = sendTelegramMessage("📡 ESP32-CAM Connection Test\n✅ Text messages work!\nIP: " + WiFi.localIP().toString());
  if (!textOK) {
    Serial.println("Text message failed!");
    telegramDebug = "❌ Text messages fail - check token/channel";
    return;
  }

  Serial.println("Text message sent successfully!");
  telegramDebug = "✅ Text messages work!";

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    telegramDebug = "✅ Text ok, ❌ camera fb null";
    return;
  }

  Serial.printf("Test photo size: %u bytes\n", (unsigned)fb->len);
  bool photoOK = sendPhotoToTelegram(fb, "Connection Test");
  esp_camera_fb_return(fb);

  if (photoOK) {
    Serial.println("Photo sent successfully!");
    telegramDebug = "✅ Both text and photos work!";
  } else {
    Serial.println("Photo failed, but text works");
    telegramDebug = "✅ Text works, ❌ Photos fail";
  }
}

// ------------ Time formatting ------------
String getTimeString() {
  unsigned long seconds = millis() / 1000UL;
  unsigned long minutes = seconds / 60UL;
  unsigned long hours = minutes / 60UL;

  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours % 24, minutes % 60, seconds % 60);
  return String(buffer);
}

String getUptimeString() {
  unsigned long seconds = millis() / 1000UL;
  unsigned long minutes = seconds / 60UL;
  unsigned long hours = minutes / 60UL;

  if (hours > 0) {
    return String(hours) + "h " + String(minutes % 60) + "m";
  } else if (minutes > 0) {
    return String(minutes) + "m " + String(seconds % 60) + "s";
  } else {
    return String(seconds) + "s";
  }
}

// ========== TELEGRAM COMMAND HANDLING ==========

long getLastUpdateID() {
  if (!SPIFFS.exists(TELEGRAM_OFFSET_FILE)) return 0;

  File file = SPIFFS.open(TELEGRAM_OFFSET_FILE, "r");
  if (!file) return 0;

  String content = file.readString();
  file.close();
  content.trim();
  return content.toInt();
}

void saveLastUpdateID(long update_id) {
  File file = SPIFFS.open(TELEGRAM_OFFSET_FILE, "w");
  if (!file) {
    Serial.println("Failed to open offset file for writing");
    return;
  }
  file.print(update_id);
  file.close();
}

String parseTelegramCommand(String message) {
  message.trim();
  if (message.indexOf('@') > 0) {
    message = message.substring(0, message.indexOf('@'));
  }
  return message;
}

static void persistSettingsDirty() {
  extern void markStatsDirty();
  markStatsDirty();
}

void handleTelegramCommand(String command) {
  Serial.println("Handling command: " + command);

  command.trim();
  command.toLowerCase();
  telegramDebug = "Command: " + command;

  if (command == "/start" || command == "/help") {
    String help = "🤖 ESP32-CAM Bot Commands:\n\n";
    help += "📸 /capture - Take photo\n";
    help += "📊 /status - Camera status\n";
    help += "🔍 /test - Test connection\n";
    help += "⚙️ /settings - Current settings\n";
    help += "🔄 /reboot - Restart camera\n";
    help += "🔧 /debug - Memory info\n";
    help += "\n--- Settings from Telegram ---\n";
    help += "🎛️ /mode 0|1|2  (0=motion,1=time,2=mixed)\n";
    help += "⏱️ /interval N  (minutes, 1..1000)\n";
    help += "🎚️ /threshold N (1000..20000)\n";
    help += "✅ /motion_on  |  ⭕ /motion_off\n";
    help += "\nIP: " + WiFi.localIP().toString();
    help += "\nUptime: " + getUptimeString();
    sendTelegramMessage(help);
  }
  else if (command == "/capture" || command == "/photo" || command == "/pic") {
    sendTelegramMessage("📸 Capturing photo...");
    captureImage("Telegram Command");
  }
  else if (command == "/status" || command == "/info") {
    String status = "📊 Camera Status:\n\n";
    status += "IP: " + WiFi.localIP().toString() + "\n";
    status += "WiFi: " + String((WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0) + " dBm\n";
    status += "Uptime: " + getUptimeString() + "\n";
    status += "Captured: " + String(capturedCount) + "\n";
    status += "Sent: " + String(sentCount) + "\n";
    status += "Mode: " + String((captureMode == 0) ? "Motion" : (captureMode == 1) ? "Time" : "Mixed");
    status += "\nInterval: " + String(timeInterval) + " min";
    status += "\nSensitivity: " + String(motionThreshold);
    status += "\nMotion: " + String(motionEnabled ? "ON" : "OFF");
    sendTelegramMessage(status);
  }
  else if (command == "/settings") {
    String settings = "⚙️ Current Settings:\n\n";
    settings += "Mode: " + String((captureMode == 0) ? "Motion" : (captureMode == 1) ? "Time" : "Mixed");
    settings += "\nInterval: " + String(timeInterval) + " min";
    settings += "\nSensitivity: " + String(motionThreshold);
    settings += "\nMotion: " + String(motionEnabled ? "ON" : "OFF");
    settings += "\nLast Capture: " + lastCaptureTime;
    settings += "\nLast Telegram: " + lastTelegramResult;
    sendTelegramMessage(settings);
  }
  else if (command == "/debug") {
    String s = "🧠 Memory Debug:\n";
    s += "freeHeap: " + String(ESP.getFreeHeap()) + "\n";
    s += "minHeap: " + String(ESP.getMinFreeHeap()) + "\n";
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM_SUPPORT)
    s += "freePSRAM: " + String(ESP.getFreePsram()) + "\n";
#else
    s += "freePSRAM: 0\n";
#endif
    s += "reset: " + resetReasonString() + " (" + String(resetReasonCode()) + ")";
    sendTelegramMessage(s);
  }
  else if (command == "/test") {
    sendTelegramMessage("🔍 Testing connection...");
    testTelegramConnection();
  }
  else if (command == "/reboot" || command == "/restart") {
    sendTelegramMessage("🔄 Restarting ESP32-CAM...");
    delay(800);
    ESP.restart();
  }
  else if (command == "/motion_on") {
    motionEnabled = true;
    persistSettingsDirty();
    sendTelegramMessage("✅ Motion detection enabled");
  }
  else if (command == "/motion_off") {
    motionEnabled = false;
    persistSettingsDirty();
    sendTelegramMessage("⭕ Motion detection disabled");
  }
  // ✅ NEW: set mode from telegram
  else if (command.startsWith("/mode ")) {
    int m = command.substring(6).toInt();
    if (m < 0 || m > 2) {
      sendTelegramMessage("❌ mode must be 0,1,2\n0=motion 1=time 2=mixed");
      return;
    }
    captureMode = m;
    persistSettingsDirty();
    sendTelegramMessage("✅ Mode set to " + String(m));
  }
  // ✅ NEW: set interval from telegram
  else if (command.startsWith("/interval ")) {
    int v = command.substring(10).toInt();
    if (v < 1 || v > 1000) {
      sendTelegramMessage("❌ interval must be 1..1000 (minutes)");
      return;
    }
    timeInterval = v;
    persistSettingsDirty();
    sendTelegramMessage("✅ Interval set to " + String(v) + " min");
  }
  // ✅ NEW: set threshold from telegram
  else if (command.startsWith("/threshold ")) {
    int v = command.substring(11).toInt();
    if (v < 1000 || v > 20000) {
      sendTelegramMessage("❌ threshold must be 1000..20000");
      return;
    }
    motionThreshold = v;
    persistSettingsDirty();
    sendTelegramMessage("✅ Threshold set to " + String(v));
  }
  else if (command == "/stream") {
    String streamUrl = "🌐 Live Stream:\n";
    streamUrl += "http://" + WiFi.localIP().toString() + "\n";
    sendTelegramMessage(streamUrl);
  }
  else {
    sendTelegramMessage("❓ Unknown command: " + command + "\nType /help");
  }
}

void checkTelegramCommands() {
  static unsigned long lastCheck = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastCheck < TELEGRAM_POLL_INTERVAL) return;
  lastCheck = currentMillis;

  if (WiFi.status() != WL_CONNECTED) return;

  long last_update_id = getLastUpdateID();

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5000);

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) +
               "/getUpdates?offset=" + String(last_update_id + 1) +
               "&limit=1&timeout=1";

  if (!http.begin(client, url)) return;

  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();

    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error && doc["ok"] == true && doc["result"].size() > 0) {
      long update_id = doc["result"][0]["update_id"];

      // Prepare command (optional)
      bool hasCmd = false;
      String cmd = "";
      String sender = "Unknown";

      // Sender info (optional)
      if (doc["result"][0].containsKey("message") &&
          doc["result"][0]["message"].containsKey("from")) {

        JsonObject from = doc["result"][0]["message"]["from"];
        if (from.containsKey("username")) {
          sender = from["username"].as<String>();
        } else if (from.containsKey("first_name")) {
          sender = from["first_name"].as<String>();
        }
      }

      // Extract text command (only if exists)
      if (doc["result"][0].containsKey("message") &&
          doc["result"][0]["message"].containsKey("text")) {

        String messageText = doc["result"][0]["message"]["text"].as<String>();
        cmd = parseTelegramCommand(messageText);
        hasCmd = (cmd.length() > 0);
      }

      // ✅ CRITICAL FIX:
      // Save update_id BEFORE executing any command (reboot/capture etc.)
      saveLastUpdateID(update_id);

      // Execute only if we actually have a text command
      if (hasCmd) {
        Serial.println("Telegram command from " + sender + ": " + cmd);
        telegramDebug = "CMD from " + sender + ": " + cmd;

        handleTelegramCommand(cmd);
      }
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


#endif
