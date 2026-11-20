#pragma once
#include "attendance-token.h"
#include <mbedtls/md.h>
inline void attendanceHmacEsp32(const uint8_t* key, size_t keyLength, const uint8_t* data, size_t length, uint8_t output[32]) {
 mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, keyLength, data, length, output);
}
// Keep the clock synced with NTP. counter = Unix seconds / 30.
// Get session UUID from the authenticated device/session transport; never use room ID in its place.
// Both QR token (counter.hex-digest) and the four-digit code MUST come from attendanceDigest.
