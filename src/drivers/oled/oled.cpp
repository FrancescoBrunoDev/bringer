/*
  oled.cpp

  Simple SSD1306 OLED helper (I2C) for showing short status messages and progress.
*/

#include "drivers/oled/oled.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Create the Adafruit display instance.
static Adafruit_SSD1306 s_oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static U8G2_FOR_ADAFRUIT_GFX s_u8g2;

// Internal availability flag and current I2C addr
static bool s_available = false;
static uint8_t s_i2c_addr = OLED_DEFAULT_I2C_ADDR;
static bool s_menu_mode = true;

// Mutex for thread safety
static SemaphoreHandle_t s_oledMutex = NULL;

#define LOCK_OLED()   if (s_oledMutex) xSemaphoreTake(s_oledMutex, portMAX_DELAY)
#define UNLOCK_OLED() if (s_oledMutex) xSemaphoreGive(s_oledMutex)

// Toast state
static String s_toast_msg;
static uint32_t s_toast_until = 0;
static uint32_t s_toast_start = 0;
static ToastPos s_toast_pos = TOAST_BOTTOM;
static ToastIcon s_toast_icon = TOAST_ICON_NONE;
static float s_toast_progress = 0.0f; 
static bool s_toast_manual = false;

void oled_setMenuMode(bool enable) {
  LOCK_OLED();
  s_menu_mode = enable;
  UNLOCK_OLED();
}

bool oled_isMenuMode(void) {
  LOCK_OLED();
  bool m = s_menu_mode;
  UNLOCK_OLED();
  return m;
}

void oled_init(uint8_t sda, uint8_t scl, uint8_t address) {
  if (s_oledMutex == NULL) {
    s_oledMutex = xSemaphoreCreateMutex();
  }
  
  LOCK_OLED();
  Wire.begin((int)sda, (int)scl);
  delay(10);

  s_i2c_addr = address;
  if (!s_oled.begin(SSD1306_SWITCHCAPVCC, s_i2c_addr)) {
    Serial.println("oled_init: SSD1306 init failed");
    s_available = false;
    UNLOCK_OLED();
    return;
  }

  s_oled.clearDisplay();
  s_oled.display();

  s_u8g2.begin(s_oled);
  s_u8g2.setFont(u8g2_font_profont12_tr);
  s_u8g2.setForegroundColor(SSD1306_WHITE);
  s_u8g2.setBackgroundColor(SSD1306_BLACK);

  s_available = true;
  Serial.println("oled_init: OK");
  UNLOCK_OLED();
}

bool oled_isAvailable(void) {
  return s_available;
}

void oled_clear(void) {
  LOCK_OLED();
  if (s_available) {
    s_oled.clearDisplay();
    s_oled.display();
  }
  UNLOCK_OLED();
}

void oled_clearBuffer(void) {
  LOCK_OLED();
  if (s_available) s_oled.clearDisplay();
  UNLOCK_OLED();
}

void oled_display(void) {
  LOCK_OLED();
  if (s_available) s_oled.display();
  UNLOCK_OLED();
}

static void _drawCenteredText(const char *msg, uint8_t textSize) {
  s_oled.clearDisplay();
  
  if (textSize >= 2) {
    s_u8g2.setFont(u8g2_font_profont17_tr);
  } else {
    s_u8g2.setFont(u8g2_font_profont12_tr);
  }

  int16_t w = s_u8g2.getUTF8Width(msg);
  int16_t x = (OLED_WIDTH - w) / 2;
  int16_t y = (OLED_HEIGHT / 2) + (s_u8g2.getFontAscent() / 2) - 2;

  s_u8g2.setCursor(x, y);
  s_u8g2.print(msg);
  s_oled.display();
}

void oled_showStatus(const char *msg) {
  LOCK_OLED();
  if (!s_available || s_menu_mode) {
    if (s_menu_mode) Serial.printf("OLED STATUS suppressed: %s\n", msg);
    UNLOCK_OLED();
    return;
  }
  _drawCenteredText(msg, 1);
  UNLOCK_OLED();
}

void oled_showLines(const char *line1, const char *line2, int16_t x_offset, int16_t y_offset, bool update) {
  LOCK_OLED();
  if (!s_available) { UNLOCK_OLED(); return; }

  s_u8g2.setFont(u8g2_font_profont11_tr);
  
  int16_t w1 = s_u8g2.getUTF8Width(line1);
  int16_t x1 = (OLED_WIDTH - w1) / 2 + x_offset;
  int16_t y1 = 20 + y_offset; 
  s_u8g2.setCursor(x1, y1);
  s_u8g2.print(line1);

  int16_t w2 = s_u8g2.getUTF8Width(line2);
  int16_t x2 = (OLED_WIDTH - w2) / 2 + x_offset;
  int16_t y2 = OLED_HEIGHT - 10 + y_offset;
  s_u8g2.setCursor(x2, y2);
  s_u8g2.print(line2);

  if (update) s_oled.display();
  UNLOCK_OLED();
}

void oled_showProgress(const char *msg, int current, int total) {
  LOCK_OLED();
  if (!s_available || s_menu_mode) {
    UNLOCK_OLED();
    return;
  }

  char buf[64];
  if (total > 0) snprintf(buf, sizeof(buf), "%s %d/%d", msg, current, total);
  else snprintf(buf, sizeof(buf), "%s", msg);
  _drawCenteredText(buf, 1);
  UNLOCK_OLED();
}

void oled_showWiFiIcon(bool connected) {
  LOCK_OLED();
  if (!s_available) { UNLOCK_OLED(); return; }
  s_oled.clearDisplay();
  int16_t cx = OLED_WIDTH / 2;
  int16_t cy = OLED_HEIGHT / 2 - 3;
  s_oled.drawCircle(cx, cy, 6, SSD1306_WHITE);
  s_oled.fillRect(0, cy + 1, OLED_WIDTH, OLED_HEIGHT - (cy + 1), SSD1306_BLACK);
  s_oled.fillCircle(cx, cy + 9, 1, SSD1306_WHITE);
  s_oled.display();
  UNLOCK_OLED();
}

void oled_drawHomeScreen(const char *time, bool wifiConnected, int16_t x_offset, int16_t y_offset, bool update) {
  LOCK_OLED();
  if (!s_available) { UNLOCK_OLED(); return; }
  
  s_u8g2.setFont(u8g2_font_logisoso24_tf); 
  int16_t w = s_u8g2.getUTF8Width(time);
  int16_t x = (OLED_WIDTH - w) / 2 + x_offset;
  int16_t y = (OLED_HEIGHT / 2) + (s_u8g2.getFontAscent() / 2) + y_offset;
  s_u8g2.setCursor(x, y);
  s_u8g2.print(time);
  
  int16_t wy_base = 5 + y_offset;
  int16_t wx_base = 120 + x_offset;
  if (wifiConnected && wy_base > -10 && wy_base < OLED_HEIGHT + 10 && wx_base > -10 && wx_base < OLED_WIDTH + 10) {
      s_oled.drawCircle(wx_base, wy_base, 4, SSD1306_WHITE);
      s_oled.fillRect(wx_base - 5, wy_base + 1, 11, 5, SSD1306_BLACK);
      s_oled.fillCircle(wx_base, wy_base + 3, 1, SSD1306_WHITE);
  } 

  if (update) s_oled.display();
  UNLOCK_OLED();
}

void oled_drawBigText(const char *text, int16_t x_offset, int16_t y_offset, bool update) {
    LOCK_OLED();
    if (!s_available) { UNLOCK_OLED(); return; }
    
    // 1. Try single line scaling first (Reduced starting size)
    s_u8g2.setFont(u8g2_font_logisoso24_tf); 
    int16_t w = s_u8g2.getUTF8Width(text);
    if (w > OLED_WIDTH - 4) {
        s_u8g2.setFont(u8g2_font_profont17_tr);
        w = s_u8g2.getUTF8Width(text);
    }

    if (w <= OLED_WIDTH - 4) {
        // Fits on one line
        int16_t h_asc = s_u8g2.getFontAscent();
        int16_t x = (OLED_WIDTH - w) / 2 + x_offset;
        int16_t y = (OLED_HEIGHT / 2) + (h_asc / 2) + y_offset;
        if (y > -44 && y < OLED_HEIGHT + 44) {
            s_u8g2.setCursor(x, y);
            s_u8g2.print(text);
        }
    } else {
        // Split into two lines
        s_u8g2.setFont(u8g2_font_profont17_tr);
        String s = text;
        int split = -1;
        int len = s.length();
        int mid = len / 2;
        
        // Find best split point (space near middle)
        for(int i=0; i < len/2; i++) {
            if(mid-i >= 0 && s[mid-i] == ' ') { split = mid-i; break; }
            if(mid+i < len && s[mid+i] == ' ') { split = mid+i; break; }
        }
        // No space? split in the middle (allow word split)
        if(split == -1) split = mid;

        String s1 = s.substring(0, split);
        String s2 = s.substring(split);
        if(s2.startsWith(" ")) s2 = s2.substring(1);

        int16_t w1 = s_u8g2.getUTF8Width(s1.c_str());
        int16_t w2 = s_u8g2.getUTF8Width(s2.c_str());
        
        // Further scaling for lines if they are still too long
        if (w1 > OLED_WIDTH - 4 || w2 > OLED_WIDTH - 4) {
            s_u8g2.setFont(u8g2_font_profont12_tr);
            w1 = s_u8g2.getUTF8Width(s1.c_str());
            w2 = s_u8g2.getUTF8Width(s2.c_str());
        }
        if (w1 > OLED_WIDTH - 4 || w2 > OLED_WIDTH - 4) {
            s_u8g2.setFont(u8g2_font_profont10_tr);
            w1 = s_u8g2.getUTF8Width(s1.c_str());
            w2 = s_u8g2.getUTF8Width(s2.c_str());
        }

        int16_t h_asc = s_u8g2.getFontAscent();
        int16_t x1 = (OLED_WIDTH - w1) / 2 + x_offset;
        int16_t x2 = (OLED_WIDTH - w2) / 2 + x_offset;
        int16_t y1 = (OLED_HEIGHT / 2) - 3 + y_offset;
        int16_t y2 = (OLED_HEIGHT / 2) + h_asc + y_offset;

        if (y1 > -44 && y1 < OLED_HEIGHT + 44) {
            s_u8g2.setCursor(x1, y1); s_u8g2.print(s1);
        }
        if (y2 > -44 && y2 < OLED_HEIGHT + 44) {
            s_u8g2.setCursor(x2, y2); s_u8g2.print(s2);
        }
    }

    if (update) s_oled.display();
    UNLOCK_OLED();
}

void oled_drawHeader(const char *title, int16_t x_offset, int16_t y_offset) {
    LOCK_OLED();
    if (!s_available || !title) { UNLOCK_OLED(); return; }

    s_u8g2.setFont(u8g2_font_profont10_tr);
    
    // Draw text (White on Black)
    s_u8g2.setForegroundColor(SSD1306_WHITE);
    s_u8g2.setBackgroundColor(SSD1306_BLACK);
    s_u8g2.setCursor(x_offset + 4, y_offset + 10);
    s_u8g2.print(title);
    
    UNLOCK_OLED();
}

void oled_drawToggle(const char *label, bool state, int16_t x_offset, int16_t y_offset) {
    LOCK_OLED();
    if (!s_available) { UNLOCK_OLED(); return; }

    // 1. Draw Label
    s_u8g2.setFont(u8g2_font_profont12_tr);
    int16_t lw = s_u8g2.getUTF8Width(label);
    int16_t lx = (OLED_WIDTH - lw) / 2 + x_offset;
    int16_t ly = (OLED_HEIGHT / 2) - 8 + y_offset;
    
    if (ly > -20 && ly < OLED_HEIGHT + 20) {
        s_u8g2.setCursor(lx, ly);
        s_u8g2.print(label);
    }

    // 2. Draw Pill Switch
    int16_t sw_w = 30;
    int16_t sw_h = 14;
    int16_t sx = (OLED_WIDTH - sw_w) / 2 + x_offset;
    int16_t sy = (OLED_HEIGHT / 2) + 4 + y_offset;

    if (sy > -20 && sy < OLED_HEIGHT + 20) {
        // Outline
        s_oled.drawRoundRect(sx, sy, sw_w, sw_h, sw_h/2, SSD1306_WHITE);
        
        if (state) {
            // ON: Fill pill and put dot on the right
            s_oled.fillRoundRect(sx, sy, sw_w, sw_h, sw_h/2, SSD1306_WHITE);
            s_oled.fillCircle(sx + sw_w - (sw_h/2) - 1, sy + (sw_h/2), (sw_h/2) - 3, SSD1306_BLACK);
        } else {
            // OFF: Just outline and put dot on the left
            s_oled.fillCircle(sx + (sw_h/2) + 1, sy + (sw_h/2), (sw_h/2) - 3, SSD1306_WHITE);
        }
    }

    UNLOCK_OLED();
}

static void _draw_toast_with_offsets(int16_t offset_x, int16_t offset_y) {
  int16_t base_y = (s_toast_pos == TOAST_TOP) ? 4 + offset_y : OLED_HEIGHT - 22 + offset_y;
  s_u8g2.setFont(u8g2_font_profont12_tr);
  int16_t text_w = s_toast_msg.length() > 0 ? s_u8g2.getUTF8Width(s_toast_msg.c_str()) : 0;
  int16_t total_w, tx;
  int16_t radius = 9;
  int16_t box_h = radius * 2;

  if (text_w == 0) {
      total_w = box_h;
      tx = OLED_WIDTH - total_w - 6 + offset_x;
      s_oled.fillCircle(tx + radius, base_y + radius, radius, SSD1306_WHITE);
  } else {
      total_w = text_w + 16;
      if (s_toast_icon != TOAST_ICON_NONE) total_w += 14;
      tx = OLED_WIDTH - total_w - 6 + offset_x;
      s_oled.fillRoundRect(tx, base_y, total_w, box_h, radius, SSD1306_WHITE);
  }
  
  s_u8g2.setForegroundColor(SSD1306_BLACK);
  s_u8g2.setBackgroundColor(SSD1306_WHITE);
  int16_t cx = tx + radius;
  int16_t cy = base_y + radius;
  
  if (s_toast_icon != TOAST_ICON_NONE) {
      switch(s_toast_icon) {
          case TOAST_ICON_UP: s_oled.fillTriangle(cx-3, cy+2, cx+3, cy+2, cx, cy-4, SSD1306_BLACK); break;
          case TOAST_ICON_DOWN: s_oled.fillTriangle(cx-3, cy-2, cx+3, cy-2, cx, cy+4, SSD1306_BLACK); break;
          case TOAST_ICON_SELECT: s_oled.fillCircle(cx, cy, 3, SSD1306_BLACK); break;
          case TOAST_ICON_BACK: s_oled.fillTriangle(cx+3, cy-3, cx+3, cy+3, cx-4, cy, SSD1306_BLACK); break;
          default: break;
      }
      cx += 14;
  }
  
  if (s_toast_msg.length() > 0) {
      s_u8g2.setCursor(cx - 3, base_y + 13);
      s_u8g2.print(s_toast_msg.c_str());
  }
  
  s_u8g2.setForegroundColor(SSD1306_WHITE);
  s_u8g2.setBackgroundColor(SSD1306_BLACK);
}

void oled_showToast(const char *msg, uint32_t ms, ToastPos pos, ToastIcon icon) {
  LOCK_OLED();
  s_toast_msg = msg ? msg : "";
  s_toast_start = millis();
  s_toast_until = s_toast_start + ms;
  s_toast_pos = pos;
  s_toast_icon = icon;
  s_toast_manual = false;
  UNLOCK_OLED();
}

void oled_showHoldToast(ToastPos pos, ToastIcon icon, float progress) {
    LOCK_OLED();
    s_toast_msg = "";
    s_toast_pos = pos;
    s_toast_icon = icon;
    s_toast_progress = progress;
    s_toast_manual = true;
    s_toast_until = millis() + 500;
    UNLOCK_OLED();
}

void oled_drawActiveToast(void) {
  LOCK_OLED();
  if (s_toast_until == 0) { UNLOCK_OLED(); return; }
  
  uint32_t now = millis();
  int16_t offset_x = 0;
  int16_t offset_y = 0;

  if (s_toast_manual) {
      offset_x = (int16_t)((1.0f - s_toast_progress) * 128.0f);
  } else {
      uint32_t remaining = s_toast_until > now ? s_toast_until - now : 0;
      const uint32_t anim_dur = 250;
      if (remaining < anim_dur) {
          float p = 1.0f - ((float)remaining / anim_dur);
          offset_y = (int16_t)(p * 20.0f);
          if (s_toast_pos == TOAST_TOP) offset_y = -offset_y;
      }
  }
  _draw_toast_with_offsets(offset_x, offset_y);
  UNLOCK_OLED();
}

bool oled_poll(void) {
  LOCK_OLED();
  if (!s_available || s_toast_until == 0) { UNLOCK_OLED(); return false; }
  if (millis() > s_toast_until) {
    s_toast_until = 0;
    s_toast_manual = false;
    UNLOCK_OLED();
    return true; 
  }
  UNLOCK_OLED();
  return true;
}

