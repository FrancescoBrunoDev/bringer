#include "../settings_internal.h"
#include "../../../common/components.h"
#include "drivers/epaper/display.h"
#include "drivers/oled/oled.h"
#include "../../../ui_internal.h"
#include <stdio.h>

enum EpdItem : uint8_t { EPD_PARTIAL = 0, EPD_FULL_CLEAN, EPD_COUNT };
static uint8_t s_index = 0;
static uint8_t s_prevIndex = 0;

static void render_item(uint8_t index, int16_t x, int16_t y) {
  switch (index) {
    case EPD_PARTIAL:
      comp_toggle("partial rendering", epd_getPartialEnabled(), x, y);
      break;
    case EPD_FULL_CLEAN:
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
    s_index = (s_index + 1) % EPD_COUNT;
    ui_triggerVerticalAnimation(true);
}

static void view_prev(void) {
    s_prevIndex = s_index;
    s_index = (s_index + EPD_COUNT - 1) % EPD_COUNT;
    ui_triggerVerticalAnimation(false);
}

static void view_select(void) {
  switch (s_index) {
    case EPD_PARTIAL: {
      bool cur = epd_getPartialEnabled();
      epd_setPartialEnabled(!cur);
      break;
    }
    case EPD_FULL_CLEAN: {
      if (!epd_forceClear_async()) {
        if (oled_isAvailable()) oled_showStatus("EPD busy");
      }
      break;
    }
  }
  ui_redraw();
}

static void view_back(void) {
    ui_setView(&VIEW_SETTINGS_MAIN);
}

static float view_get_progress(void) {
    return (float)(s_index + 1) / (float)EPD_COUNT;
}

const View VIEW_SETTINGS_EPAPER = {
    "Settings > E-Paper",
    view_render,
    view_next,
    view_prev,
    view_select,
    view_back,
    NULL,
    view_get_progress
};
