/*
  oled.cpp

  Simple SSD1306 OLED helper (I2C) for showing short status messages and progress.

  - Uses Adafruit_SSD1306 + Adafruit_GFX
  - Provides:
      - oled_init(sda, scl, address)
      - oled_isAvailable()
      - oled_clear()
      - oled_showStatus(msg)        // large centered text
      - oled_showLines(line1,line2) // two-line display
      - oled_showProgress(msg, cur, total)
*/

#include "epaper_monitor/oled/oled.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <stdio.h>

// Create the Adafruit display instance.
// Use -1 as reset pin (most breakouts don't expose a RST pin).
static Adafruit_SSD1306 s_oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Internal availability flag and current I2C addr
static bool s_available = false;
static uint8_t s_i2c_addr = OLED_DEFAULT_I2C_ADDR;
// When the UI menu is active we suppress e-paper status/progress messages on the OLED
// so the menu view remains exclusive.
static bool s_menu_mode = true;

// Toast state (unused if toasts disabled)
static String s_toast_msg;
static uint32_t s_toast_until = 0;

void oled_setMenuMode(bool enable) {
  s_menu_mode = enable;
}

bool oled_isMenuMode(void) {
  return s_menu_mode;
}

void oled_init(uint8_t sda, uint8_t scl, uint8_t address) {
  // Initialize I2C with chosen pins
  Wire.begin((int)sda, (int)scl);
  delay(10);

  s_i2c_addr = address;
  // Try to initialize the SSD1306
  if (!s_oled.begin(SSD1306_SWITCHCAPVCC, s_i2c_addr)) {
    // Initialization failed: mark unavailable and print a message
    Serial.println("oled_init: SSD1306 init failed");
    s_available = false;
    return;
  }

  // Basic setup
  s_oled.clearDisplay();
  s_oled.setTextColor(SSD1306_WHITE);
  s_oled.setTextSize(1);
  s_oled.display();

  s_available = true;
  Serial.println("oled_init: OK");
}

bool oled_isAvailable(void) {
  return s_available;
}

void oled_clear(void) {
  if (!s_available) {
    Serial.println("oled_clear: OLED not available");
    return;
  }
  s_oled.clearDisplay();
  s_oled.display();
}

// Helper: draw centered text at a given size
static void _drawCenteredText(const char *msg, uint8_t textSize) {
  s_oled.clearDisplay();
  s_oled.setTextSize(textSize);
  s_oled.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  // getTextBounds signature: (str, x, y, &x1, &y1, &w, &h)
  s_oled.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);

  int16_t cx = (int16_t)((OLED_WIDTH - w) / 2) - x1;
  int16_t cy = (int16_t)((OLED_HEIGHT - h) / 2) - y1;
  s_oled.setCursor(cx, cy);
  s_oled.print(msg);
  s_oled.display();
}

// Show a short status message (big, centered)
void oled_showStatus(const char *msg) {
  if (!s_available) {
    Serial.print("OLED STATUS: ");
    Serial.println(msg);
    return;
  }
  // If the UI is using the OLED exclusively, suppress e-paper status messages.
  if (s_menu_mode) {
    Serial.print("OLED STATUS suppressed (menu mode): ");
    Serial.println(msg);
    return;
  }
  // Use larger font for status (size 2); fallback to size 1 if text too wide
  _drawCenteredText(msg, 2);
}

bool oled_poll(void) {
  if (!s_available) return false;
  if (s_toast_until && millis() > s_toast_until) {
    s_toast_until = 0;
    // Redraw: do nothing here — caller should call ui_draw() after oled_poll
    return true;
  }
  return false;
}

// Two-line display: useful for IP + status
void oled_showLines(const char *line1, const char *line2) {
  if (!s_available) {
    Serial.print("OLED LINES: ");
    Serial.print(line1);
    Serial.print(" / ");
    Serial.println(line2);
    return;
  }

  s_oled.clearDisplay();
  s_oled.setTextSize(1);
  s_oled.setTextColor(SSD1306_WHITE);

  // Line 1: near top (centered)
  int16_t x1, y1;
  uint16_t w, h;
  s_oled.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (int16_t)((OLED_WIDTH - w) / 2) - x1;
  int16_t y = 8; // small top margin
  s_oled.setCursor(x, y);
  s_oled.print(line1);

  // Line 2: near bottom (centered)
  s_oled.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
  x = (int16_t)((OLED_WIDTH - w) / 2) - x1;
  y = OLED_HEIGHT - h - 8; // small bottom margin
  s_oled.setCursor(x, y);
  s_oled.print(line2);

  s_oled.display();
}

// Show message + progress (e.g. 'Clearing 1/4')
void oled_showProgress(const char *msg, int current, int total) {
  if (!s_available) {
    Serial.print("OLED PROGRESS: ");
    Serial.print(msg);
    Serial.print(" ");
    Serial.print(current);
    Serial.print("/");
    Serial.println(total);
    return;
  }

  // If the UI is using the OLED exclusively, suppress progress from other modules.
  if (s_menu_mode) {
    Serial.print("OLED PROGRESS suppressed (menu mode): ");
    Serial.print(msg);
    Serial.print(" ");
    Serial.print(current);
    Serial.print("/");
    Serial.println(total);
    return;
  }

  char buf[64];
  if (total > 0) {
    snprintf(buf, sizeof(buf), "%s %d/%d", msg, current, total);
  } else {
    snprintf(buf, sizeof(buf), "%s", msg);
  }
  // Use a slightly smaller size to accommodate the extra text
  _drawCenteredText(buf, 2);
}

void oled_showWifiIcon(bool connected) {
  if (!s_available) {
    Serial.print("OLED WIFI ICON: ");
    Serial.println(connected ? "connected" : "disconnected");
    return;
  }
  s_oled.clearDisplay();
  // Even simpler and smaller icon: single arc + dot
  int16_t cx = OLED_WIDTH / 2;
  int16_t cy = OLED_HEIGHT / 2 - 3;

  // One small circle to represent the arc
  s_oled.drawCircle(cx, cy, 6, SSD1306_WHITE);
  // Mask bottom half to get a semicircle
  s_oled.fillRect(0, cy + 1, OLED_WIDTH, OLED_HEIGHT - (cy + 1), SSD1306_BLACK);
  // tiny dot below
  s_oled.fillCircle(cx, cy + 9, 1, SSD1306_WHITE);
  s_oled.display();
}

void oled_showToast(const char *msg, uint32_t ms) {
  if (!s_available) {
    Serial.print("OLED TOAST: ");
    Serial.println(msg);
    return;
  }
  s_toast_msg = String(msg);
  s_toast_until = millis() + ms;

  // Draw overlay: small filled rect near bottom with text
  s_oled.fillRect(4, OLED_HEIGHT - 18, OLED_WIDTH - 8, 14, SSD1306_WHITE);
  s_oled.setTextColor(SSD1306_BLACK);
  s_oled.setTextSize(1);
  int16_t x1, y1; uint16_t w, h;
  s_oled.getTextBounds(s_toast_msg.c_str(), 0, 0, &x1, &y1, &w, &h);
  int16_t x = (int16_t)((OLED_WIDTH - w) / 2) - x1;
  int16_t y = OLED_HEIGHT - 16;
  s_oled.setCursor(x, y);
  s_oled.print(s_toast_msg);
  s_oled.display();
}
