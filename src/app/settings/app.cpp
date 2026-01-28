#include "settings.h"
#include "app/ui/common/components.h"
#include "app/ui/ui_internal.h"
#include "drivers/epaper/display.h"
#include "drivers/oled/oled.h"
#include <stdio.h>
#include <Arduino.h>

extern const App APP_SETTINGS;

enum SettingsItem : uint8_t { SET_WIFI = 0, SET_EPAPER, SET_COUNT };
static uint8_t s_index = 0;
static uint8_t s_prevIndex = 0;

static void render_item(uint8_t index, int16_t x, int16_t y) {
  switch (index) {
    case SET_WIFI:
      oled_drawBigText("WIFI", x, y, false);
      break;
    case SET_EPAPER:
      oled_drawBigText("E-PAPER", x, y, false);
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
    case SET_WIFI:
      ui_setView(&VIEW_SETTINGS_WIFI);
      break;
    case SET_EPAPER:
      ui_setView(&VIEW_SETTINGS_EPAPER);
      break;
  }
  ui_redraw();
}

static void view_back(void) {
    ui_setView(NULL);
}

static float view_get_progress(void) {
    return (float)(s_index + 1) / (float)SET_COUNT;
}

const View VIEW_SETTINGS_MAIN = {
    .title = "Settings",
    .render = view_render,
    .onNext = view_next,
    .onPrev = view_prev,
    .onSelect = view_select,
    .onBack = view_back,
    .poll = NULL,
    .getScrollProgress = view_get_progress
};

static void app_renderPreview(int16_t x_offset, int16_t y_offset) {
    comp_title_and_text("Settings", "", x_offset, y_offset, false);
}

static void app_select(void) {
    s_index = 0;
    ui_setView(&VIEW_SETTINGS_MAIN);
}

const App APP_SETTINGS = {
    .name = "Settings",
    .renderPreview = app_renderPreview,
    .onSelect = app_select,
    .setup = nullptr,
    .registerRoutes = nullptr,
    .poll = nullptr
};
