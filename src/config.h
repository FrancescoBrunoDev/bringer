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
#ifdef BOARD_SEEED_XIAO_ESP32C6
// Seeed Studio XIAO ESP32C6 Pinout
constexpr uint8_t PIN_SCK  = 19; // D8
constexpr uint8_t PIN_MISO = 255; // Not used (Repurposed for Button Confirm)
constexpr uint8_t PIN_MOSI = 18; // D10

// Display control
constexpr uint8_t PIN_CS   = 16; // D6
constexpr uint8_t PIN_BUSY = 21; // D3
constexpr uint8_t PIN_RST  = 2;  // D2
constexpr uint8_t PIN_DC   = 1;  // D1

// OLED Display
constexpr uint8_t PIN_OLED_SDA = 22; // D4
constexpr uint8_t PIN_OLED_SCL = 23; // D5
constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// Input Controls
constexpr uint8_t PIN_BUTTON_PREV    = 0;  // D0
constexpr uint8_t PIN_BUTTON_NEXT    = 17; // D7
constexpr uint8_t PIN_BUTTON_CONFIRM = 20; // D9 (Repurposed MISO)

#else
// Default (ESP32-C6 DevKitM-1 or generic)
// SCK, MISO, MOSI - these are the pins used in SPI.begin(sck, miso, mosi, ss)
constexpr uint8_t PIN_SCK  = 18;
constexpr uint8_t PIN_MISO = 19;
constexpr uint8_t PIN_MOSI = 23;

// Display control pins (change if your wiring differs)
constexpr uint8_t PIN_CS   = 5;
constexpr uint8_t PIN_BUSY = 4;
constexpr uint8_t PIN_RST  = 2;
constexpr uint8_t PIN_DC   = 21;

// --- OLED Display ---
#ifndef PIN_OLED_SDA
constexpr uint8_t PIN_OLED_SDA = 15;
#endif
#ifndef PIN_OLED_SCL
constexpr uint8_t PIN_OLED_SCL = 22;
#endif
#ifndef OLED_I2C_ADDR
constexpr uint8_t OLED_I2C_ADDR = 0x3C;
#endif

// --- Input Controls (Buttons) ---
#ifndef PIN_BUTTON_PREV
constexpr uint8_t PIN_BUTTON_PREV = 11;
#endif
#ifndef PIN_BUTTON_NEXT
constexpr uint8_t PIN_BUTTON_NEXT = 9;
#endif
#ifndef PIN_BUTTON_CONFIRM
constexpr uint8_t PIN_BUTTON_CONFIRM = 10;
#endif

#endif

// HTTP server configuration
constexpr uint16_t WEB_SERVER_PORT = 80;

// Behavior flags (tweak for your panel / use-case)
constexpr bool ENABLE_FORCE_CLEAR    = true; // run recovery clear at startup if true
constexpr bool ENABLE_PARTIAL_UPDATE = false;  // attempt partial updates when supported

// Misc
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL; // how long to wait for STA connect