/*
 * base64.cpp
 *
 * Small, dependency-light base64 decoder for the ESP32 e-paper API example.
 *
 * See base64.h for the public API.
 */

#include "base64.h"

static inline int _b64val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

bool base64_decode(const String &in, std::vector<uint8_t> &out) {
  out.clear();
  int val = 0, valb = -8;
  for (unsigned int i = 0; i < in.length(); ++i) {
    int c = _b64val(in[i]);
    if (c == -1) {
      // '=' padding -> stop decoding
      if (in[i] == '=') break;
      // ignore other non-base64 chars (whitespace/newlines/etc)
      else continue;
    }
    val = (val << 6) + c;
    valb += 6;
    if (valb >= 0) {
      out.push_back((uint8_t)((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return true;
}