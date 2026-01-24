#include "apps.h"
#include "../common/components.h"
#include "../ui_internal.h"
#include "app/wifi/wifi.h"
#include "drivers/epaper/display.h"
#include "drivers/oled/oled.h"
#include <stdio.h>

enum SettingsItem : uint8_t { SET_IP = 0, SET_PARTIAL, SET_FULL_CLEAN, SET_COUNT };
static uint8_t s_index = 0;

static void view_render(int16_t x_offset, int16_t y_offset) {
  char buf[40];
  switch (s_index) {
    case SET_IP: {
      IPAddress ip = wifi_getIP();
      if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
        comp_title_and_text("Settings", "IP: none", x_offset, y_offset, false);
      } else {
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        comp_title_and_text("Settings", buf, x_offset, y_offset, false);
      }
      break;
    }
    case SET_PARTIAL:
      snprintf(buf, sizeof(buf), "Partial: %s", epd_getPartialEnabled() ? "ON" : "OFF");
      comp_title_and_text("Settings", buf, x_offset, y_offset, false);
      break;
    case SET_FULL_CLEAN:
        comp_title_and_text("Settings", "Full cleaning", x_offset, y_offset, false);
      break;
    default:
        comp_title_and_text("Settings", "", x_offset, y_offset, false);
      break;
  }
}

static void view_next(void) {
    s_index = (s_index + 1) % SET_COUNT;
    ui_redraw();
}

static void view_prev(void) {
    s_index = (s_index + SET_COUNT - 1) % SET_COUNT;
    ui_redraw();
}

static void view_select(void) {
  switch (s_index) {
    case SET_IP:
      break;
    case SET_PARTIAL: {
      bool cur = epd_getPartialEnabled();
      epd_setPartialEnabled(!cur);
      delay(600);
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
