/*
  config.h
  Configuration constants for the ESP32 e-paper monitor example.

  - Pin assignments
  - Feature flags
  - General constants
*/

#pragma once

#include <Arduino.h>

// SPI pins (explicitly set to ensure consistent SPI.begin(...) usage)
// SCK, MISO, MOSI - these are the pins used in SPI.begin(sck, miso, mosi, ss)
constexpr uint8_t PIN_SCK  = 18;
constexpr uint8_t PIN_MISO = 19;
constexpr uint8_t PIN_MOSI = 23;

// Display control pins (change if your wiring differs)
constexpr uint8_t PIN_CS   = 5;
constexpr uint8_t PIN_BUSY = 4;
constexpr uint8_t PIN_RST  = 2;
constexpr uint8_t PIN_DC   = 21;

// HTTP server configuration
constexpr uint16_t WEB_SERVER_PORT = 80;

// Behavior flags (tweak for your panel / use-case)
constexpr bool ENABLE_FORCE_CLEAR    = false; // run recovery clear at startup if true
constexpr bool ENABLE_PARTIAL_UPDATE = true;  // attempt partial updates when supported

// Misc
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL; // how long to wait for STA connect