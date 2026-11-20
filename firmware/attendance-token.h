#ifndef ATTENDANCE_TOKEN_H
#define ATTENDANCE_TOKEN_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
// Supply HMAC-SHA256 using the board crypto library. Secret is raw bytes, not hex text.
typedef void (*AttendanceHmac)(const uint8_t*, size_t, const uint8_t*, size_t, uint8_t[32]);
inline bool attendanceDigest(const uint8_t* secret, size_t secretLength, const char* session,
                             uint64_t counter, AttendanceHmac hmac, uint8_t output[32]) {
 if (strlen(session) != 36) return false;
 const char* prefix = "attendance:v1:"; // 14 bytes + UUID 36 bytes + colon 1 byte + counter 8 bytes
 const size_t prefixLength = strlen(prefix);
 // 14 + 36 + 1 + 8 = 59 bytes (use exact buffer below).
 uint8_t payload[59];
 memcpy(payload, prefix, prefixLength);
 memcpy(payload + prefixLength, session, 36);
 payload[prefixLength + 36] = ':';
 for (size_t i = 0; i < 8; ++i) payload[prefixLength + 37 + i] = (uint8_t)(counter >> (56 - 8*i));
 hmac(secret, secretLength, payload, sizeof(payload), output);
 return true;
}
inline void attendanceCode(const uint8_t digest[32], char output[5]) {
 const size_t offset = digest[31] & 15;
 const uint32_t binary = ((uint32_t)(digest[offset] & 127) << 24) |
    ((uint32_t)digest[offset+1] << 16) | ((uint32_t)digest[offset+2] << 8) | digest[offset+3];
 snprintf(output, 5, "%04u", (unsigned)(binary % 10000));
}
#endif
