// functions.h (ONE SERVO VERSION)
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <ESP32Servo.h>

// ===== Persisted struct =====
static const uint8_t PERSIST_MAGIC = 0xA7;
struct Persisted {
  uint8_t magic;
  uint8_t mode;         // 0..2
  uint16_t intervalMin; // 1..1000
  uint8_t motionEn;     // 0/1
  uint16_t threshold;   // 1000..20000
  uint32_t captured;
  uint32_t sent;
  int16_t pan;
};

// ===== Reset reason (portable) =====
static int resetReasonCode() {
#if defined(ESP_ARDUINO_VERSION_MAJOR)
  return (int)esp_reset_reason();
#else
  return 0;
#endif
}
static String resetReasonString() {
  return String(resetReasonCode());
}

// ===== Servo (PAN only) =====
#if SERVO_ENABLED
static Servo servoPan;
static int panAngle = PAN_CENTER;

static int clampi(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void servoWriteRaw(int pan) {
  servoPan.write(pan);
}

static void servoApply(bool smooth = true) {
  panAngle = clampi(panAngle, PAN_MIN, PAN_MAX);

  if (!smooth) {
    servoWriteRaw(panAngle);
    return;
  }

  int cur = servoPan.read();
  if (cur <= 0) cur = panAngle;

  int guard = 0;
  while (cur != panAngle) {
    if (cur < panAngle) cur++;
    else cur--;
    servoWriteRaw(cur);
    delay(7);
    if (++guard > 300) break;
  }
}

void servoCenter(bool smooth) {
  panAngle = PAN_CENTER;
  servoApply(smooth);
  stageSettingsDirty();
}

void servoInit() {
  servoPan.setPeriodHertz(50);
  servoPan.attach(SERVO_PAN_PIN, 500, 2400);
  servoCenter(false);
  Serial.printf("Servo init OK. pan=%d\n", panAngle);
}
#else
void servoInit() {}
void servoCenter(bool smooth) { (void)smooth; }
#endif

// ===== Time Tehran =====
void setupTimeTehran() {
  const char* TZ_TEHRAN = "IRST-3:30";
  configTzTime(TZ_TEHRAN, "pool.ntp.org", "time.google.com", "time.windows.com");
  struct tm t;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&t, 500)) {
      Serial.printf("Time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
      return;
    }
    delay(500);
  }
  Serial.println("NTP sync pending...");
}

String getTimeString() {
  struct tm t;
  if (getLocalTime(&t, 50)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return String(buf);
  }
  unsigned long s = millis() / 1000UL;
  unsigned long m = s / 60UL;
  unsigned long h = m / 60UL;
  char buf[24];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu (uptime)", h % 24, m % 60, s % 60);
  return String(buf);
}

String getUptimeString() {
  unsigned long s = millis() / 1000UL;
  unsigned long m = s / 60UL;
  unsigned long h = m / 60UL;
  if (h > 0) return String(h) + "h " + String(m % 60) + "m";
  if (m > 0) return String(m) + "m " + String(s % 60) + "s";
  return String(s) + "s";
}

// ===== Camera =====
bool initializeCamera() {
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    c.frame_size = FRAMESIZE_SVGA;
    c.jpeg_quality = 10;
    c.fb_count = 2;
  } else {
    c.frame_size = FRAMESIZE_VGA;
    c.jpeg_quality = 12;
    c.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("esp_camera_init failed: 0x%x\n", (int)err);
    return false;
  }
  Serial.println("Camera initialized");
  return true;
}

// ===== WiFi =====
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.printf("Connecting to %s", SSID);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi OK: " + WiFi.localIP().toString());
  } else {
    Serial.println("WiFi FAILED");
  }
}

// ===== EEPROM persistence =====
void loadSettings() {
  Persisted p;
  EEPROM.get(0, p);

  if (p.magic != PERSIST_MAGIC) {
    Serial.println("EEPROM empty -> defaults");
    captureMode = 0;
    timeInterval = 5;
    motionEnabled = true;
    motionThreshold = 5000;
    capturedCount = 0;
    sentCount = 0;
#if SERVO_ENABLED
    panAngle = PAN_CENTER;
#endif
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

#if SERVO_ENABLED
  if (p.pan >= 0 && p.pan <= 180) panAngle = p.pan;
#endif

  Serial.println("EEPROM loaded");
}

void saveSettingsNow() {
  Persisted p;
  p.magic = PERSIST_MAGIC;
  p.mode = (uint8_t)captureMode;
  p.intervalMin = (uint16_t)timeInterval;
  p.motionEn = (uint8_t)(motionEnabled ? 1 : 0);
  p.threshold = (uint16_t)motionThreshold;
  p.captured = (uint32_t)capturedCount;
  p.sent = (uint32_t)sentCount;
#if SERVO_ENABLED
  p.pan = (int16_t)panAngle;
#else
  p.pan = -1;
#endif

  EEPROM.put(0, p);
  EEPROM.commit();
  Serial.println("EEPROM committed");
}

// ===== Motion (simple heuristic) =====
bool detectMotion() {
  if (!motionEnabled) return false;

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;

  if (!previousFrame) {
    previousFrame = (uint8_t*)malloc(fb->len);
    if (previousFrame) {
      memcpy(previousFrame, fb->buf, fb->len);
      previousFrameSize = fb->len;
    }
    esp_camera_fb_return(fb);
    return false;
  }

  long diff = labs((long)fb->len - (long)previousFrameSize);
  bool motion = (diff > motionThreshold);

  if (motion) {
    if (fb->len > previousFrameSize) {
      uint8_t* q = (uint8_t*)realloc(previousFrame, fb->len);
      if (!q) {
        esp_camera_fb_return(fb);
        return false;
      }
      previousFrame = q;
    }
    memcpy(previousFrame, fb->buf, fb->len);
    previousFrameSize = fb->len;
  }

  esp_camera_fb_return(fb);
  return motion;
}

// ===== Telegram (text) =====
bool sendTelegramMessage(const String& text) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(12000);

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";
  if (!http.begin(client, url)) return false;

  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;
  doc["chat_id"] = String(TELEGRAM_CHAT_ID);
  doc["text"] = text;

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);
  http.end();

  return code == 200;
}

// ===== Telegram (photo streaming) =====
static bool readHttpResponse(WiFiClientSecure& c, String& out) {
  unsigned long start = millis();
  out = "";
  while (c.connected() && (millis() - start) < 15000UL) {
    while (c.available()) {
      char ch = (char)c.read();
      out += ch;
      if (out.length() > 3500) out.remove(0, 1500);
    }
    if (!c.available()) delay(10);
  }
  return out.length() > 0;
}

bool sendPhotoToTelegram(camera_fb_t* fb, const String& caption) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60000);

  const char* host = "api.telegram.org";
  if (!client.connect(host, 443)) return false;

  String boundary = "----ESP32CAM" + String(millis());

  String part1 =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
    String(TELEGRAM_CHAT_ID) + "\r\n";

  String part2 =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
    caption + "\r\n";

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
    size_t n = left > CHUNK ? CHUNK : left;
    size_t w = client.write(p, n);
    if (w == 0) { client.stop(); return false; }
    p += w; left -= w;
    delay(0);
  }

  client.print(tail);

  String response;
  readHttpResponse(client, response);
  client.stop();

  bool ok200 = response.indexOf(" 200 ") >= 0;
  bool okJson = response.indexOf("\"ok\":true") >= 0;
  return ok200 && okJson;
}

// ===== Capture =====
void captureImage(const String& type) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    lastCaptureTime = "Failed";
    lastCaptureType = type;
    telegramDebug = "fb null";
    return;
  }

  capturedCount++;
  lastCaptureTime = getTimeString();
  lastCaptureType = type;

  String cap = "ESP32-CAM (" + type + ")\n" + lastCaptureTime;

  bool ok = sendPhotoToTelegram(fb, cap);
  if (ok) {
    sentCount++;
    lastTelegramResult = "OK @ " + getTimeString();
    telegramDebug = "photo sent";
  } else {
    lastTelegramResult = "FAIL @ " + getTimeString();
    telegramDebug = "send failed";
  }

  esp_camera_fb_return(fb);
  lastCaptureMillis = millis();

  stageSettingsDirty();
}

// ===== Telegram test =====
void testTelegramConnection() {
  sendTelegramMessage("📡 Test\nIP: " + WiFi.localIP().toString() + "\nTime: " + getTimeString());
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return;
  sendPhotoToTelegram(fb, "Test photo\n" + getTimeString());
  esp_camera_fb_return(fb);
}

// ===== Telegram offset file =====
static long getLastUpdateID() {
  if (!SPIFFS.exists(TELEGRAM_OFFSET_FILE)) return 0;
  File f = SPIFFS.open(TELEGRAM_OFFSET_FILE, "r");
  if (!f) return 0;
  String s = f.readString();
  f.close();
  s.trim();
  return s.toInt();
}
static void saveLastUpdateID(long id) {
  File f = SPIFFS.open(TELEGRAM_OFFSET_FILE, "w");
  if (!f) return;
  f.print(id);
  f.close();
}

String parseTelegramCommand(String s) {
  s.trim();
  if (s.indexOf('@') > 0) s = s.substring(0, s.indexOf('@'));
  return s;
}

// ===== Telegram commands (PAN only) =====
void handleTelegramCommand(String cmd) {
  cmd.toLowerCase();

  if (cmd == "/start" || cmd == "/help") {
    String h = "🤖 Commands:\n";
    h += "/capture\n/status\n/settings\n/test\n/stream\n/reboot\n/motion_on\n/motion_off\n/debug\n";
#if SERVO_ENABLED
    h += "/left /right /center\n";
    h += "/pan N   (0..180)\n/servo\n";
#endif
    h += "\nIP: " + WiFi.localIP().toString();
    h += "\nTime: " + getTimeString();
    sendTelegramMessage(h);
  }
  else if (cmd == "/capture" || cmd == "/photo" || cmd == "/pic") {
    sendTelegramMessage("📸 Capturing...");
    captureImage("Telegram");
  }
  else if (cmd == "/status" || cmd == "/info") {
    String s = "📊 Status:\n";
    s += "IP: " + WiFi.localIP().toString() + "\n";
    s += "Uptime: " + getUptimeString() + "\n";
    s += "Now: " + getTimeString() + "\n";
    s += "Captured: " + String(capturedCount) + "\n";
    s += "Sent: " + String(sentCount) + "\n";
    s += "Mode: " + String(captureMode) + "\n";
    s += "Interval: " + String(timeInterval) + "m\n";
    s += "Threshold: " + String(motionThreshold) + "\n";
#if SERVO_ENABLED
    s += "Pan: " + String(panAngle) + "\n";
#endif
    sendTelegramMessage(s);
  }
  else if (cmd == "/settings") {
    String s = "⚙️ Settings:\n";
    s += "mode=" + String(captureMode) + "\n";
    s += "interval=" + String(timeInterval) + "\n";
    s += "motion=" + String(motionEnabled ? "on" : "off") + "\n";
    s += "threshold=" + String(motionThreshold) + "\n";
    sendTelegramMessage(s);
  }
  else if (cmd == "/test") {
    testTelegramConnection();
  }
  else if (cmd == "/stream") {
    sendTelegramMessage("🌐 Stream:\nhttp://" + WiFi.localIP().toString());
  }
  else if (cmd == "/motion_on") {
    motionEnabled = true;
    stageSettingsDirty();
    sendTelegramMessage("✅ motion enabled");
  }
  else if (cmd == "/motion_off") {
    motionEnabled = false;
    stageSettingsDirty();
    sendTelegramMessage("⭕ motion disabled");
  }
  else if (cmd == "/debug") {
    String s = "🧠 Debug:\n";
    s += "heap=" + String(ESP.getFreeHeap()) + "\n";
    s += "minHeap=" + String(ESP.getMinFreeHeap()) + "\n";
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM_SUPPORT)
    s += "psram=" + String(ESP.getFreePsram()) + "\n";
#else
    s += "psram=0\n";
#endif
    s += "resetCode=" + resetReasonString() + "\n";
    s += "uptime=" + getUptimeString() + "\n";
    s += "now=" + getTimeString() + "\n";
    sendTelegramMessage(s);
  }
  else if (cmd == "/reboot" || cmd == "/restart") {
    static unsigned long lastRebootAt = 0;
    unsigned long now = millis();
    if (now - lastRebootAt < 15000) {
      sendTelegramMessage("⚠️ Reboot already in progress");
      return;
    }
    lastRebootAt = now;

    sendTelegramMessage("🔄 Restarting...");
    delay(900);
    ESP.restart();
  }

#if SERVO_ENABLED
  else if (cmd == "/servo") {
    sendTelegramMessage("🎛 Servo\nPan=" + String(panAngle));
  }
  else if (cmd == "/center") {
    servoCenter(true);
    sendTelegramMessage("🎯 Centered\nPan=" + String(panAngle));
  }
  else if (cmd == "/left") {
    panAngle = clampi(panAngle - SERVO_STEP, PAN_MIN, PAN_MAX);
    servoApply(true);
    stageSettingsDirty();
    sendTelegramMessage("⬅️ Pan=" + String(panAngle));
  }
  else if (cmd == "/right") {
    panAngle = clampi(panAngle + SERVO_STEP, PAN_MIN, PAN_MAX);
    servoApply(true);
    stageSettingsDirty();
    sendTelegramMessage("➡️ Pan=" + String(panAngle));
  }
  else if (cmd.startsWith("/pan ")) {
    int a = cmd.substring(5).toInt();
    panAngle = clampi(a, PAN_MIN, PAN_MAX);
    servoApply(true);
    stageSettingsDirty();
    sendTelegramMessage("🎚 Pan=" + String(panAngle));
  }
#endif
  else {
    sendTelegramMessage("❓ Unknown. Use /help");
  }
}

void checkTelegramCommands() {
  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (now - lastCheck < TELEGRAM_POLL_INTERVAL) return;
  lastCheck = now;

  if (WiFi.status() != WL_CONNECTED) return;

  long lastId = getLastUpdateID();

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(6000);

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN)
             + "/getUpdates?offset=" + String(lastId + 1)
             + "&limit=1&timeout=1";

  if (!http.begin(client, url)) return;

  int code = http.GET();
  if (code == 200) {
    String resp = http.getString();
    StaticJsonDocument<1536> doc;
    if (!deserializeJson(doc, resp) && doc["ok"] == true && doc["result"].size() > 0) {

      long update_id = doc["result"][0]["update_id"];

      // IMPORTANT: save offset BEFORE executing command
      saveLastUpdateID(update_id);

      if (doc["result"][0].containsKey("message") && doc["result"][0]["message"].containsKey("text")) {
        String txt = doc["result"][0]["message"]["text"].as<String>();
        String cmd = parseTelegramCommand(txt);

        telegramDebug = "CMD: " + cmd;
        Serial.println("TG cmd: " + cmd);

        if (cmd.length()) handleTelegramCommand(cmd);
      }
    }
  }
  http.end();
}

// ===== Web routes =====
void setupServerRoutes() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", INDEX_HTML); });

  server.on("/stream", HTTP_GET, []() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { server.send(500, "text/plain", "fb null"); return; }
    server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
  });

  server.on("/capture-now", HTTP_GET, []() {
    captureImage("Web Manual");
    server.send(200, "text/plain", "Capture triggered");
  });

  server.on("/test-telegram", HTTP_GET, []() {
    testTelegramConnection();
    server.send(200, "text/plain", "Telegram test sent");
  });

  server.on("/save-settings", HTTP_POST, []() {
    String body = server.arg("plain");
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body)) { server.send(400, "text/plain", "Bad JSON"); return; }

    int m = doc["mode"] | 0;
    int ti = doc["interval"] | 5;
    int th = doc["threshold"] | 5000;

    if (m < 0 || m > 2) m = 0;
    if (ti < 1) ti = 1; if (ti > 1000) ti = 1000;
    if (th < 1000) th = 1000; if (th > 20000) th = 20000;

    captureMode = m;
    timeInterval = ti;
    motionThreshold = th;

    stageSettingsDirty();
    server.send(200, "text/plain", "OK (will persist soon)");
  });

  server.on("/servo", HTTP_GET, []() {
#if SERVO_ENABLED
    if (server.hasArg("cmd")) {
      String c = server.arg("cmd");
      c.toLowerCase();

      if (c == "left") { panAngle = clampi(panAngle - SERVO_STEP, PAN_MIN, PAN_MAX); servoApply(true); }
      else if (c == "right") { panAngle = clampi(panAngle + SERVO_STEP, PAN_MIN, PAN_MAX); servoApply(true); }
      else if (c == "center") { servoCenter(true); }

      stageSettingsDirty();
      server.send(200, "text/plain", "OK");
      return;
    }

    if (server.hasArg("pan")) {
      panAngle = clampi(server.arg("pan").toInt(), PAN_MIN, PAN_MAX);
      servoApply(true);
      stageSettingsDirty();
      server.send(200, "text/plain", "OK");
      return;
    }

    server.send(400, "text/plain", "Missing args");
#else
    server.send(200, "text/plain", "SERVO_DISABLED");
#endif
  });

  server.on("/debug", HTTP_GET, []() {
    StaticJsonDocument<768> doc;
    doc["heap"] = ESP.getFreeHeap();
    doc["minHeap"] = ESP.getMinFreeHeap();
#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_SPIRAM_SUPPORT)
    doc["psram"] = ESP.getFreePsram();
#else
    doc["psram"] = 0;
#endif
    doc["resetCode"] = resetReasonCode();
    doc["uptime"] = getUptimeString();
    doc["now"] = getTimeString();
    doc["ip"] = WiFi.localIP().toString();

    String out;
    serializeJsonPretty(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/status", HTTP_GET, []() {
    StaticJsonDocument<1024> doc;
    doc["capturedCount"] = capturedCount;
    doc["sentCount"] = sentCount;
    doc["lastCaptureTime"] = lastCaptureTime;
    doc["lastCaptureType"] = lastCaptureType;
    doc["lastTelegramResult"] = lastTelegramResult;
    doc["captureMode"] = captureMode;
    doc["timeInterval"] = timeInterval;
    doc["motionThreshold"] = motionThreshold;
    doc["uptime"] = getUptimeString();
    doc["nowTime"] = getTimeString();
    doc["telegramDebug"] = telegramDebug;
    doc["currentMode"] = (captureMode==0)?"Motion":(captureMode==1)?"Time":"Mixed";
#if SERVO_ENABLED
    doc["pan"] = panAngle;
#else
    doc["pan"] = -1;
#endif

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.begin();
  Serial.println("HTTP server started");
}

#endif
