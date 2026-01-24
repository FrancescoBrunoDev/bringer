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

#include "drivers/oled/oled.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>

#include <stdio.h>

// Create the Adafruit display instance.
// Use -1 as reset pin (most breakouts don't expose a RST pin).
static Adafruit_SSD1306 s_oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static U8G2_FOR_ADAFRUIT_GFX s_u8g2;

// Internal availability flag and current I2C addr
static bool s_available = false;
static uint8_t s_i2c_addr = OLED_DEFAULT_I2C_ADDR;
// When the UI menu is active we suppress e-paper status/progress messages on the OLED
// so the menu view remains exclusive.
static bool s_menu_mode = true;

// Toast state
static String s_toast_msg;
static uint32_t s_toast_until = 0;
static uint32_t s_toast_start = 0;
static ToastPos s_toast_pos = TOAST_BOTTOM;
static ToastIcon s_toast_icon = TOAST_ICON_NONE;

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
  s_oled.display(); // clear immediately

  // Init U8g2
  s_u8g2.begin(s_oled);
  // Fonts: https://github.com/olikraus/u8g2/wiki/fntlistall
  // User requested ProFont
  s_u8g2.setFont(u8g2_font_profont12_tr);
  s_u8g2.setForegroundColor(SSD1306_WHITE);
  s_u8g2.setBackgroundColor(SSD1306_BLACK);

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

void oled_clearBuffer(void) {
  if (s_available) s_oled.clearDisplay();
}

void oled_display(void) {
  if (s_available) s_oled.display();
}

// Helper: draw centered text
// Uses U8g2 for drawing. Note: textSize arg is ignored as we rely on specific fonts now.
static void _drawCenteredText(const char *msg, uint8_t textSize) {
  s_oled.clearDisplay();
  
  // Select font based on "size" hint
  // Size 2 originally meant "large status". Size 1 meant "normal".
  if (textSize >= 2) {
    s_u8g2.setFont(u8g2_font_profont17_tr); // Larger
  } else {
    s_u8g2.setFont(u8g2_font_profont12_tr); // Standard
  }

  int16_t w = s_u8g2.getUTF8Width(msg);
  int16_t h = s_u8g2.getFontAscent() - s_u8g2.getFontDescent(); // Approx height
  
  // Center X
  int16_t x = (OLED_WIDTH - w) / 2;
  // Center Y (U8g2 prints at baseline). 
  // Baseline should be at cy + (ascent/2).
  int16_t y = (OLED_HEIGHT / 2) + (s_u8g2.getFontAscent() / 2) - 2;

  s_u8g2.setCursor(x, y);
  s_u8g2.print(msg);
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
  _drawCenteredText(msg, 1);
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
  
  // Use a smaller readable font for multi-line
  s_u8g2.setFont(u8g2_font_profont11_tr);
  
  int16_t h = s_u8g2.getFontAscent();
  
  // Line 1: Top half
  int16_t w1 = s_u8g2.getUTF8Width(line1);
  int16_t x1 = (OLED_WIDTH - w1) / 2;
  int16_t y1 = 20; // approximate top baseline

  s_u8g2.setCursor(x1, y1);
  s_u8g2.print(line1);

  // Line 2: Bottom half
  int16_t w2 = s_u8g2.getUTF8Width(line2);
  int16_t x2 = (OLED_WIDTH - w2) / 2;
  int16_t y2 = OLED_HEIGHT - 10; // approximate bottom baseline

  s_u8g2.setCursor(x2, y2);
  s_u8g2.print(line2);

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
  _drawCenteredText(buf, 1);
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

void oled_drawHomeScreen(const char *time, bool wifiConnected, int16_t y_offset, bool update) {
  if (!s_available) return;
  
  // Draw Time: Large, centered
  s_u8g2.setFont(u8g2_font_logisoso32_tf); 
  int16_t w = s_u8g2.getUTF8Width(time);
  int16_t h_asc = s_u8g2.getFontAscent();
  int16_t x = (OLED_WIDTH - w) / 2;
  int16_t y = (OLED_HEIGHT / 2) + (h_asc / 2) + y_offset;

  s_u8g2.setCursor(x, y);
  s_u8g2.print(time);
  
  // WiFi Icon (Small, top right) if visible
  int16_t wy_base = 5 + y_offset;
  if (wifiConnected && wy_base > -10 && wy_base < OLED_HEIGHT + 10) {
      int16_t wx = 120; 
      s_oled.drawCircle(wx, wy_base, 4, SSD1306_WHITE);
      s_oled.fillRect(wx - 5, wy_base + 1, 11, 5, SSD1306_BLACK);
      s_oled.fillCircle(wx, wy_base + 3, 1, SSD1306_WHITE);
  } 

  if (update) s_oled.display();
}

void oled_drawBigText(const char *text, int16_t y_offset, bool update) {
    if (!s_available) return;
    
    // Choose font based on text length (fallback if too long for logisoso32)
    s_u8g2.setFont(u8g2_font_logisoso32_tf); 
    int16_t w = s_u8g2.getUTF8Width(text);
    if (w > OLED_WIDTH - 4) {
        s_u8g2.setFont(u8g2_font_logisoso24_tf);
        w = s_u8g2.getUTF8Width(text);
    }
    if (w > OLED_WIDTH - 4) {
        s_u8g2.setFont(u8g2_font_profont17_tr);
        w = s_u8g2.getUTF8Width(text);
    }
    
    int16_t h_asc = s_u8g2.getFontAscent();
    int16_t x = (OLED_WIDTH - w) / 2;
    int16_t y = (OLED_HEIGHT / 2) + (h_asc / 2) + y_offset;

    // Boundary check for rendering performance/glitches
    if (y < -32 || y > OLED_HEIGHT + 32) return;

    s_u8g2.setCursor(x, y);
    s_u8g2.print(text);

    if (update) s_oled.display();
}

static void _draw_toast_internal(int16_t offset) {
  int16_t base_y = (s_toast_pos == TOAST_TOP) ? 4 + offset : OLED_HEIGHT - 22 + offset;
  
  s_u8g2.setFont(u8g2_font_profont12_tr);
  int16_t text_w = s_toast_msg.length() > 0 ? s_u8g2.getUTF8Width(s_toast_msg.c_str()) : 0;
  
  int16_t total_w, tx;
  int16_t radius = 9;
  int16_t box_h = radius * 2;

  if (text_w == 0) {
      total_w = box_h;
      tx = OLED_WIDTH - total_w - 6;
      s_oled.fillCircle(tx + radius, base_y + radius, radius, SSD1306_WHITE);
  } else {
      total_w = text_w + 16;
      if (s_toast_icon != TOAST_ICON_NONE) total_w += 14;
      tx = OLED_WIDTH - total_w - 6;
      s_oled.fillRoundRect(tx, base_y, total_w, box_h, radius, SSD1306_WHITE);
  }
  
  s_u8g2.setForegroundColor(SSD1306_BLACK);
  s_u8g2.setBackgroundColor(SSD1306_WHITE);
  
  int16_t cx = tx + radius;
  int16_t cy = base_y + radius;
  
  // Draw Icon in BLACK (smaller)
  if (s_toast_icon != TOAST_ICON_NONE) {
      switch(s_toast_icon) {
          case TOAST_ICON_UP:
              s_oled.fillTriangle(cx-3, cy+2, cx+3, cy+2, cx, cy-4, SSD1306_BLACK);
              break;
          case TOAST_ICON_DOWN:
              s_oled.fillTriangle(cx-3, cy-2, cx+3, cy-2, cx, cy+4, SSD1306_BLACK);
              break;
          case TOAST_ICON_SELECT:
              s_oled.fillCircle(cx, cy, 3, SSD1306_BLACK);
              break;
          default: break;
      }
      cx += 14;
  }
  
  // Draw Text
  if (s_toast_msg.length() > 0) {
      s_u8g2.setCursor(cx - 3, base_y + 13);
      s_u8g2.print(s_toast_msg.c_str());
  }
  
  // Restore colors
  s_u8g2.setForegroundColor(SSD1306_WHITE);
  s_u8g2.setBackgroundColor(SSD1306_BLACK);
}

void oled_showToast(const char *msg, uint32_t ms, ToastPos pos, ToastIcon icon) {
  if (!s_available) return;
  s_toast_msg = msg ? msg : "";
  s_toast_start = millis();
  s_toast_until = s_toast_start + ms;
  s_toast_pos = pos;
  s_toast_icon = icon;
}

void oled_drawActiveToast(void) {
  if (s_toast_until == 0) return;
  
  uint32_t now = millis();
  uint32_t remaining = s_toast_until > now ? s_toast_until - now : 0;
  
  const uint32_t anim_dur = 250;
  int16_t offset = 0;
  
  if (remaining < anim_dur) {
      // Slide out: from 0 to 20
      float p = 1.0f - ((float)remaining / anim_dur);
      offset = (int16_t)(p * 20.0f);
  }
  
  // If at top, offset should be negative (slide up to -20)
  if (s_toast_pos == TOAST_TOP) {
      _draw_toast_internal(-offset);
  } else {
      _draw_toast_internal(offset);
  }
}

bool oled_poll(void) {
  if (!s_available) return false;
  if (s_toast_until == 0) return false;
  
  if (millis() > s_toast_until) {
    s_toast_until = 0;
    return true; // Redraw to clear
  }
  
  // Keep redrawing for animations
  return true;
}
