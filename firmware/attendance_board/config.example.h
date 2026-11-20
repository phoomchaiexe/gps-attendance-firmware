// คัดลอกไฟล์นี้เป็น config.h แล้วกรอกค่าจริง (config.h ถูก .gitignore กันไว้ ห้ามอัปขึ้น GitHub)
#pragma once

#define WIFI_SSID "ชื่อ-wifi"
#define WIFI_PASSWORD "รหัสผ่าน-wifi"

#define NTP_SERVER "pool.ntp.org"

// ที่อยู่เว็บแอปจริง ไม่มี / ท้ายสุด เช่น "https://your-app.vercel.app"
// มือถือต้องเปิดผ่าน HTTPS ถึงจะใช้กล้อง/GPS ได้
#define APP_BASE_URL "https://example.com"

// ต้องตรงกับ device_id ในตาราง devices (ตัวอักษร ตัวเลข _ - ยาวไม่เกิน 64)
#define DEVICE_ID "board-05-0901"

// secret ของบอร์ด hex 64 ตัวอักษร (สร้างตามขั้นตอนในหัวไฟล์ attendance_board.ino) ห้ามเปิดเผย
#define DEVICE_SECRET_HEX "0000000000000000000000000000000000000000000000000000000000000000"

// รหัสสำหรับสั่งตั้งคาบผ่าน HTTP ใช้ในเครือข่ายภายในเท่านั้น (เป็น HTTP ธรรมดา ไม่ได้เข้ารหัส)
#define ADMIN_TOKEN "เปลี่ยนเป็นรหัสยาว-สุ่ม"
