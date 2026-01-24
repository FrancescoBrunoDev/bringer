#include "apps.h"
#include "../common/components.h"
#include "../ui_internal.h"
#include "app/wifi/wifi.h"
#include "drivers/epaper/display.h"
#include "drivers/oled/oled.h"
#include <stdio.h>

enum SettingsItem : uint8_t { SET_IP = 0, SET_PARTIAL, SET_FULL_CLEAN, SET_COUNT };
static uint8_t s_index = 0;
static uint8_t s_prevIndex = 0;

static void render_item(uint8_t index, int16_t x, int16_t y) {
  char buf[40];
  switch (index) {
    case SET_IP: {
      IPAddress ip = wifi_getIP();
      if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
        oled_drawBigText("IP: none", x, y, false);
      } else {
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        oled_drawBigText(buf, x, y, false);
      }
      break;
    }
    case SET_PARTIAL:
      snprintf(buf, sizeof(buf), "Partial: %s", epd_getPartialEnabled() ? "ON" : "OFF");
      oled_drawBigText(buf, x, y, false);
      break;
    case SET_FULL_CLEAN:
      oled_drawBigText("Full clean", x, y, false);
      break;
  }
}

static void view_render(int16_t x_offset, int16_t y_offset) {
  if (abs(y_offset) < 1) {
    render_item(s_index, x_offset, 0);
  } else {
    render_item(s_index, x_offset, y_offset);
    render_item(s_prevIndex, x_offset, y_offset > 0 ? y_offset - 64 : y_offset + 64);
  }
}

static void view_next(void) {
    s_prevIndex = s_index;
    s_index = (s_index + 1) % SET_COUNT;
    ui_triggerVerticalAnimation(true);
}

static void view_prev(void) {
    s_prevIndex = s_index;
    s_index = (s_index + SET_COUNT - 1) % SET_COUNT;
    ui_triggerVerticalAnimation(false);
}

static void view_select(void) {
  switch (s_index) {
    case SET_IP:
      break;
    case SET_PARTIAL: {
      bool cur = epd_getPartialEnabled();
      epd_setPartialEnabled(!cur);
      break;
    }
    case SET_FULL_CLEAN: {
      if (!epd_forceClear_async()) {
        if (oled_isAvailable()) oled_showStatus("EPD busy");
      }
      break;
    }
  }
  ui_redraw();
}

static void view_back(void) {
    ui_setView(NULL);
}

static const View VIEW_SETTINGS = {
    "Settings",
    view_render,
    view_next,
    view_prev,
    view_select,
    view_back,
    NULL
};

static void app_renderPreview(int16_t x_offset, int16_t y_offset) {
    comp_title_and_text("Settings", "", x_offset, y_offset, false);
}

static void app_select(void) {
    s_index = 0;
    ui_setView(&VIEW_SETTINGS);
}

const App APP_SETTINGS = {
    "Settings",
    app_renderPreview,
    app_select,
    NULL
};
