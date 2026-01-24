#include "apps.h"
#include "../common/components.h"
#include "../ui_internal.h"
#include "app/routes/text_app/text_app.h"
#include "drivers/oled/oled.h"
#include "drivers/epaper/display.h"
#include <GxEPD2_3C.h>

static uint8_t s_index = 0;

static void view_render(int16_t x_offset, int16_t y_offset) {
    const char *txt = text_app_get_text(s_index);
    if (txt) {
        comp_title_and_text("Text App", txt, x_offset, y_offset, false);
    } else {
        comp_title_and_text("Text App", "(no options)", x_offset, y_offset, false);
    }
}

static void view_next(void) {
    size_t count = text_app_get_count();
    if (count == 0) return;
    s_index = (s_index + 1) % (uint8_t)count;
    ui_redraw();
}

static void view_prev(void) {
    size_t count = text_app_get_count();
    if (count == 0) return;
    s_index = (s_index + (uint8_t)count - 1) % (uint8_t)count;
    ui_redraw();
}

static void view_select(void) {
    const char *txt = text_app_get_text(s_index);
    if (!txt) {
        if (oled_isAvailable()) oled_showToast("No options", 1000);
        return;
    }

    if (epd_isBusy()) {
        if (oled_isAvailable()) oled_showToast("EPD busy", 1000);
        return;
    }

    if (oled_isAvailable()) oled_showToast("Rendering...", 1200);
    epd_displayText(String(txt), GxEPD_RED, false);
    if (oled_isAvailable()) oled_showToast("Done", 800);
    
    // Stay in the menu
    ui_redraw();
}

static void view_back(void) {
    ui_setView(NULL); // Return to carousel
}

static const View VIEW_TEXT = {
    view_render,
    view_next,
    view_prev,
    view_select,
    view_back,
    NULL // poll
};

static void app_renderPreview(int16_t x_offset, int16_t y_offset) {
    comp_title_and_text("Text App", "", x_offset, y_offset, false);
}

static void app_select(void) {
    s_index = 0;
    ui_setView(&VIEW_TEXT);
}

const App APP_TEXT = {
    "Text",
    app_renderPreview,
    app_select,
    NULL
};
