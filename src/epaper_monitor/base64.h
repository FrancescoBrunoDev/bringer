#pragma once

#include <Arduino.h>
#include <vector>
#include <cstdint>

/*
 * base64.h
 *
 * Small, dependency-light base64 decoder for the ESP32 e-paper API example.
 *
 * Behavior:
 *  - Accepts an Arduino `String` containing base64-encoded data.
 *  - Ignores non-base64 characters (whitespace/newlines/etc).
 *  - Stops at '=' padding characters.
 *  - Writes decoded raw bytes into `out` (the vector is cleared first).
 *
 * Returns:
 *  - `true` on successful decode (bytes written into `out`).
 *  - `false` on unrecoverable errors (for e.g., incorrect padding).
 *
 * Implementation note:
 *  - Keep the implementation small and robust for embedded environments.
 */

bool base64_decode(const String &in, std::vector<uint8_t> &out);

// Convenience wrapper for C strings (optional)
inline bool base64_decode_cstr(const char *s, std::vector<uint8_t> &out) {
  return base64_decode(String(s), out);
}