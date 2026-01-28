#include "settings.h"
#include "app/wifi/wifi.h"
#include "drivers/oled/oled.h"
#include "app/ui/ui_internal.h"
#include <stdio.h>
#include <Arduino.h>

enum WifiItem : uint8_t { WIFI_SSID_INFO = 0, WIFI_IP_INFO, WIFI_COUNT };
static uint8_t s_index = 0;
static uint8_t s_prevIndex = 0;

static void render_item(uint8_t index, int16_t x, int16_t y) {
  char buf[40];
  switch (index) {
    case WIFI_SSID_INFO:
      snprintf(buf, sizeof(buf), "SSID:%s", wifi_getSSID().c_str());
      oled_drawBigText(buf, x, y, false);
      break;
    case WIFI_IP_INFO: {
      IPAddress ip = wifi_getIP();
      snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      oled_drawBigText(buf, x, y, false);
      break;
    }
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
    s_index = (s_index + 1) % WIFI_COUNT;
    ui_triggerVerticalAnimation(true);
}

static void view_prev(void) {
    s_prevIndex = s_index;
    s_index = (s_index + WIFI_COUNT - 1) % WIFI_COUNT;
    ui_triggerVerticalAnimation(false);
}

static void view_back(void) {
    ui_setView(&VIEW_SETTINGS_MAIN);
}

static float view_get_progress(void) {
    return (float)(s_index + 1) / (float)WIFI_COUNT;
}

const View VIEW_SETTINGS_WIFI = {
    .title = "Settings > Wifi",
    .render = view_render,
    .onNext = view_next,
    .onPrev = view_prev,
    .onSelect = NULL,
    .onBack = view_back,
    .poll = NULL,
    .getScrollProgress = view_get_progress
};
