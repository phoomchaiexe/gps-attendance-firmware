/*
 * เข้าห้อง: เฟิร์มแวร์บอร์ด ESP32-S3
 *
 * ทำอะไร: ซิงก์เวลา NTP แล้วสร้างรหัสเช็คชื่อ 4 หลัก + ลิงก์ QR ที่เปลี่ยนทุก 30 วินาที
 * ใช้โปรโตคอลเดียวกับเว็บ (attendance:v1) ผ่าน ../attendance-token-esp32.h
 *
 * การตั้งค่า
 *   1. คัดลอก config.example.h เป็น config.h แล้วกรอกค่า (config.h ถูก .gitignore กันไว้ ห้ามอัปขึ้น GitHub)
 *   2. Arduino IDE: เลือกบอร์ด "ESP32S3 Dev Module" (ติดตั้ง esp32 by Espressif v3.x)
 *   3. อัปโหลด แล้วเปิด Serial Monitor ที่ 115200
 *
 * ผูกบอร์ดกับห้องในฐานข้อมูล (ทำครั้งเดียว) เว็บจะใช้ secret ของบอร์ดตรวจรหัสคาบที่เปิดในห้องนั้น
 *   สร้าง secret + ค่าที่เข้ารหัส (รันที่โฟลเดอร์เว็บแอป ใช้ DEVICE_ENCRYPTION_KEY ตัวเดียวกับที่เว็บใช้):
 *     DEVICE_ENCRYPTION_KEY=<hex 64 ตัว> node -e "import('./src/lib/attendance/tokens.mjs').then(async m=>{const {randomBytes}=await import('node:crypto');const s=randomBytes(32);console.log('DEVICE_SECRET_HEX='+s.toString('hex'));console.log('secret_encrypted='+m.encryptSecret(s,process.env.DEVICE_ENCRYPTION_KEY));})"
 *   นำ DEVICE_SECRET_HEX ใส่ config.h แล้วบันทึกลงตาราง devices:
 *     insert into public.devices(device_id, room_id, secret_encrypted)
 *     values ('board-05-0901', '<room_id ของห้อง>', '<secret_encrypted>');
 *
 * บอกบอร์ดว่าตอนนี้คาบไหน (ตอนนี้เว็บยังไม่มี API สำหรับบอร์ด จึงตั้ง UUID ของคาบเองได้ 2 ทาง)
 *   - Serial:  session <uuid>   |  clear  |  status
 *   - HTTP:    curl -X POST -H "X-Admin-Token: <ADMIN_TOKEN>" -d "uuid=<uuid>" http://<ip-บอร์ด>/session
 *   UUID ของคาบดูได้จากลิงก์เช็คชื่อ /check-in?session=<uuid>
 *
 * หมายเหตุ: โค้ดนี้ยังไม่ได้ทดสอบกับบอร์ดจริง การแสดงผลตอนนี้ออกทาง Serial ถ้ามีจอ
 * (OLED/TFT) ให้ใส่โค้ดวาดใน showCode() และถ้าอยากแสดงเป็น QR ใช้ไลบรารี QR แล้วส่งค่า url เข้าไป
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include "config.h"
#include "../attendance-token-esp32.h"

static const uint32_t PERIOD_SECONDS = 30;
static const time_t MIN_VALID_TIME = 1700000000;  // ถ้าเวลาน้อยกว่านี้ แปลว่ายังซิงก์ NTP ไม่เสร็จ

static uint8_t secret[32];
static char sessionId[37] = "";
static uint64_t lastCounter = UINT64_MAX;
static bool timeWasSynced = false;
static uint32_t lastWifiAttempt = 0;
static String serialLine;

static Preferences prefs;
static WebServer server(80);

static int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool hexToBytes(const char* hex, uint8_t* out, size_t length) {
  if (strlen(hex) != length * 2) return false;
  for (size_t i = 0; i < length; ++i) {
    int high = hexValue(hex[2 * i]), low = hexValue(hex[2 * i + 1]);
    if (high < 0 || low < 0) return false;
    out[i] = (uint8_t)(high * 16 + low);
  }
  return true;
}

static bool validUuid(const char* s) {
  if (strlen(s) != 36) return false;
  for (int i = 0; i < 36; ++i) {
    bool dash = (i == 8 || i == 13 || i == 18 || i == 23);
    if (dash ? s[i] != '-' : hexValue(s[i]) < 0) return false;
  }
  return true;
}

// ตั้ง UUID ของคาบ (แปลงเป็นตัวพิมพ์เล็กให้ตรงกับที่เว็บใช้) แล้วจำไว้ใน NVS เผื่อบอร์ดรีสตาร์ท
static bool setSession(const char* uuid) {
  if (!validUuid(uuid)) return false;
  for (int i = 0; i < 36; ++i) sessionId[i] = (char)tolower((unsigned char)uuid[i]);
  sessionId[36] = '\0';
  prefs.putString("session", sessionId);
  lastCounter = UINT64_MAX;  // บังคับคำนวณและแสดงรหัสใหม่ทันที
  return true;
}

static void clearSession() {
  sessionId[0] = '\0';
  prefs.remove("session");
  lastCounter = UINT64_MAX;
}

// แสดงรหัสให้นักศึกษาเห็น ตอนนี้ออก Serial ถ้ามีจอให้เพิ่มโค้ดวาดตรงนี้
static void showCode(const char* code, const char* url, uint64_t counter) {
  Serial.println();
  Serial.printf("[รหัสเช็คชื่อ] %s  (counter %llu, เปลี่ยนใน <=%u วินาที)\n", code, (unsigned long long)counter, PERIOD_SECONDS);
  Serial.printf("[ลิงก์ QR]     %s\n", url);
}

static void showIdle(const char* message) {
  Serial.printf("[รอ] %s\n", message);
}

static void printStatus() {
  time_t now = time(nullptr);
  Serial.printf("wifi=%s ip=%s time=%s session=%s device=%s\n",
                WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
                WiFi.localIP().toString().c_str(),
                now >= MIN_VALID_TIME ? "synced" : "not-synced",
                sessionId[0] ? sessionId : "(none)", DEVICE_ID);
}

static void handleSerialLine(String line) {
  line.trim();
  if (line.startsWith("session ")) {
    String uuid = line.substring(8);
    uuid.trim();
    Serial.println(setSession(uuid.c_str()) ? "ตั้งค่าคาบแล้ว" : "UUID ไม่ถูกต้อง");
  } else if (line == "clear") {
    clearSession();
    Serial.println("ล้างคาบแล้ว");
  } else if (line == "status") {
    printStatus();
  } else if (line.length()) {
    Serial.println("คำสั่ง: session <uuid> | clear | status");
  }
}

static bool authorized() {
  return server.header("X-Admin-Token") == String(ADMIN_TOKEN);
}

static void setupServer() {
  const char* headers[] = {"X-Admin-Token"};
  server.collectHeaders(headers, 1);
  // ไม่มี endpoint ไหนคืนรหัสหรือ secret ออกไป รหัสแสดงบนบอร์ดเท่านั้น
  server.on("/status", HTTP_GET, []() {
    server.send(200, "application/json",
                String("{\"device\":\"") + DEVICE_ID + "\",\"time_synced\":" +
                    (time(nullptr) >= MIN_VALID_TIME ? "true" : "false") + ",\"session_set\":" +
                    (sessionId[0] ? "true" : "false") + "}");
  });
  server.on("/session", HTTP_POST, []() {
    if (!authorized()) { server.send(401, "text/plain", "unauthorized"); return; }
    bool ok = setSession(server.arg("uuid").c_str());
    server.send(ok ? 200 : 400, "text/plain", ok ? "ok" : "invalid uuid");
  });
  server.on("/session", HTTP_DELETE, []() {
    if (!authorized()) { server.send(401, "text/plain", "unauthorized"); return; }
    clearSession();
    server.send(200, "text/plain", "cleared");
  });
  server.begin();
}

static void connectWifi() {
  lastWifiAttempt = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nเข้าห้อง firmware (ESP32-S3)");

  if (!hexToBytes(DEVICE_SECRET_HEX, secret, sizeof(secret))) {
    Serial.println("DEVICE_SECRET_HEX ต้องเป็น hex 64 ตัวอักษร (ตรวจ config.h) หยุดทำงาน");
    while (true) delay(1000);
  }

  prefs.begin("attendance", false);
  String saved = prefs.getString("session", "");
  if (saved.length()) setSession(saved.c_str());

  connectWifi();
  configTime(0, 0, NTP_SERVER);  // เก็บเวลาเป็น UTC counter ใช้ Unix time จึงไม่ขึ้นกับเขตเวลา
  setupServer();
  printStatus();
}

void loop() {
  server.handleClient();

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') { handleSerialLine(serialLine); serialLine = ""; }
    else if (serialLine.length() < 80) serialLine += c;
  }

  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttempt > 10000) connectWifi();

  time_t now = time(nullptr);
  if (now < MIN_VALID_TIME) {  // ยังไม่ได้เวลาจาก NTP ห้ามสร้างรหัสจากเวลาผิด
    static uint32_t lastNotice = 0;
    if (millis() - lastNotice > 5000) { lastNotice = millis(); showIdle("รอซิงก์เวลา NTP"); }
    return;
  }
  if (!timeWasSynced) { timeWasSynced = true; Serial.println("ซิงก์เวลาสำเร็จ"); }

  if (!sessionId[0]) {
    static uint32_t lastNotice = 0;
    if (millis() - lastNotice > 10000) { lastNotice = millis(); showIdle("ยังไม่ได้ตั้งคาบ (session <uuid>)"); }
    return;
  }

  uint64_t counter = (uint64_t)now / PERIOD_SECONDS;
  if (counter == lastCounter) return;
  lastCounter = counter;

  uint8_t digest[32];
  if (!attendanceDigest(secret, sizeof(secret), sessionId, counter, attendanceHmacEsp32, digest)) {
    showIdle("UUID ของคาบไม่ถูกต้อง");
    return;
  }

  char code[5];
  attendanceCode(digest, code);

  // QR token = counter + "." + digest hex ตรงกับ qrAt() ในเว็บ
  char hex[65];
  for (int i = 0; i < 32; ++i) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
  char url[360];
  snprintf(url, sizeof(url), "%s/c?session=%s&token=%llu.%s", APP_BASE_URL, sessionId,
           (unsigned long long)counter, hex);

  showCode(code, url, counter);
}
