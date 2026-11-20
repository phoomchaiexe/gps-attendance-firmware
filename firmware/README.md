# เข้าห้อง: Firmware (ESP32-S3)

ส่วนอุปกรณ์ของระบบเช็คชื่อเข้าเรียนด้วย GPS "เข้าห้อง" บอร์ด **ESP32-S3** ที่ติดในห้องเรียนทำหน้าที่สร้างรหัสเช็คชื่อที่เปลี่ยนทุก 30 วินาที เพื่อให้นักศึกษาเช็คชื่อได้เฉพาะตอนอยู่ในห้องจริง

ส่วนเว็บแอปอยู่อีก repo: **[ใส่ลิงก์ Application repo]**

## หลักการทำงาน

1. บอร์ดซิงก์เวลาจาก NTP เพื่อให้นาฬิกาตรงกับเซิร์ฟเวอร์
2. คำนวณ `counter = Unix time (วินาที) / 30` ดังนั้นค่า counter เปลี่ยนทุก 30 วินาที
3. คำนวณ HMAC-SHA256 จากข้อความ `attendance:v1:` + UUID ของคาบเรียน (36 ตัวอักษร) + `:` + counter (8 ไบต์ big-endian) โดยใช้ device secret ของบอร์ดเป็นกุญแจ
4. แปลงผลลัพธ์เป็น **รหัส 4 หลัก** (dynamic truncation แบบ HOTP: ใช้ไบต์สุดท้ายเป็น offset แล้วหารเอาเศษด้วย 10000) และเป็น **QR token** (`counter.hex-digest`)
5. เซิร์ฟเวอร์คำนวณด้วยสูตรเดียวกัน และยอมรับ counter ปัจจุบันกับก่อนหน้า 1 ช่วงเท่านั้น จึงทนความเหลื่อมของนาฬิกาได้เล็กน้อย

ถ้ารหัสหลุดออกนอกห้อง รหัสจะหมดอายุภายในไม่เกิน 60 วินาที และเว็บยังตรวจพิกัด GPS ซ้ำอีกชั้น

## ไฟล์ในรีโป

| ไฟล์ | หน้าที่ |
|---|---|
| `firmware/attendance-token.h` | ไลบรารี C++ สร้าง digest และรหัส 4 หลัก ไม่ผูกกับฮาร์ดแวร์ (รับฟังก์ชัน HMAC จากภายนอก) |
| `firmware/attendance-token-esp32.h` | ตัวเชื่อม HMAC-SHA256 ด้วย mbedTLS ของ ESP32 |
| `firmware/token-vectors.json` | ชุดค่าทดสอบ (golden vectors) ที่ใช้ตรวจว่าบอร์ดกับเว็บได้ผลตรงกัน |
| `tests/firmware-token.cpp` | โปรแกรมทดสอบบนคอมพิวเตอร์ เทียบผลกับ `token-vectors.json` |

## การใช้งานบน ESP32-S3 (ตัวอย่าง)

```cpp
#include "attendance-token-esp32.h"

uint8_t digest[32];
char code[5];
uint64_t counter = time(nullptr) / 30;   // ต้องซิงก์ NTP ก่อน

if (attendanceDigest(secret, 32, sessionUuid, counter, attendanceHmacEsp32, digest)) {
  attendanceCode(digest, code);          // code = "0000"-"9999"
}
```

ข้อควรระวัง
- `secret` เป็นไบต์ดิบ ไม่ใช่ข้อความ hex
- UUID ของคาบต้องมาจากระบบ ห้ามใช้ room ID แทน
- QR token และรหัส 4 หลักต้องมาจาก `attendanceDigest` ตัวเดียวกัน

## การทดสอบ

```sh
# ต้องมี C++ compiler และ OpenSSL (หรือ CommonCrypto บน macOS)
g++ tests/firmware-token.cpp -o /tmp/fw-test -lcrypto
/tmp/fw-test 12345678-1234-4234-8234-123456789abc 0 2349
```

โปรแกรมคืนค่า 0 เมื่อรหัสตรงกับค่าใน `token-vectors.json` (counter 0 ต้องได้รหัส `2349`)

## ฮาร์ดแวร์

- บอร์ด: ESP32-S3
- การแสดงผล: **[ใส่ว่าใช้จอ/LED อะไรแสดงรหัส]**
- การต่อสาย: **[ใส่รูปหรือตารางขาที่ใช้]**
- เครือข่าย: Wi-Fi (ใช้ซิงก์เวลา NTP และรับรหัสคาบ)

## ผู้จัดทำ

**[ใส่ชื่อทีม/สมาชิก]**
