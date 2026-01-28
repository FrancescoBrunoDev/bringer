#include "beszel.h"
#include "app/ui/ui_internal.h"
#include "app/ui/common/types.h"
#include "app/ui/common/components.h"
#include "drivers/oled/oled.h"
#include "drivers/epaper/display.h"
#include "drivers/epaper/layout.h"
#include <GxEPD2_3C.h>
#include <Arduino.h>

extern const App APP_BESZEL;

// State
static uint8_t s_index = 0;
static uint8_t s_prevIndex = 0;
static unsigned long s_lastFetch = 0;
static const unsigned long FETCH_INTERVAL = 30000; // 30 seconds

static void fetch_data() {
    if (oled_isAvailable()) oled_showToast("Fetching Beszel...", 1000);
    if (BeszelService::getInstance().fetchSystems()) {
        if (oled_isAvailable()) oled_showToast("Data Updated", 800);
    } else {
        if (oled_isAvailable()) oled_showToast("Fetch Failed", 1500);
    }
    s_lastFetch = millis();
}

static void render_system_item(uint8_t index, int16_t x, int16_t y) {
    const auto& systems = BeszelService::getInstance().getSystems();
    if (index < systems.size()) {
        const auto& sys = systems[index];
        char buf[32];
        snprintf(buf, sizeof(buf), "%s", sys.name.c_str());
        oled_drawBigText(buf, x, y, false);
    } else {
        oled_drawBigText("No Systems", x, y, false);
    }
}

static void view_render(int16_t x_offset, int16_t y_offset) {
    auto count = BeszelService::getInstance().getSystemCount();
    if (count == 0) {
        oled_drawBigText("No Data", x_offset, y_offset, false);
        return;
    }

    if (abs(y_offset) < 1) {
        render_system_item(s_index, x_offset, 0);
    } else {
        render_system_item(s_index, x_offset, y_offset);
        render_system_item(s_prevIndex, x_offset, y_offset > 0 ? y_offset - 64 : y_offset + 64);
    }
}

static void update_epaper(const BeszelSystem& sys) {
    if (epd_isBusy()) {
        if (oled_isAvailable()) oled_showToast("EPD busy", 1000);
        return;
    }

    EpdPage page;
    page.title = "Beszel Hub";
    
    // System Info Header
    page.components.push_back({EPD_COMP_HEADER, sys.name, "", 0, GxEPD_BLACK});
    page.components.push_back({EPD_COMP_ROW, "Status", sys.status, 0, sys.status == "up" ? GxEPD_BLACK : GxEPD_RED});
    page.components.push_back({EPD_COMP_SEPARATOR, "", "", 0, 0});
    
    // Usage Stats
    page.components.push_back({EPD_COMP_PROGRESS, "CPU", String(sys.cpu, 1) + "%", sys.cpu, sys.cpu > 80 ? GxEPD_RED : GxEPD_BLACK});
    page.components.push_back({EPD_COMP_PROGRESS, "Memory", String(sys.mem, 1) + "%", sys.mem, sys.mem > 85 ? GxEPD_RED : GxEPD_BLACK});
    page.components.push_back({EPD_COMP_PROGRESS, "Disk", String(sys.disk, 1) + "%", sys.disk, sys.disk > 90 ? GxEPD_RED : GxEPD_BLACK});
    
    // Network Info
    page.components.push_back({EPD_COMP_SEPARATOR, "", "", 0, 0});
    String netStr = String(sys.net / 1024.0f, 1) + " KB/s";
    page.components.push_back({EPD_COMP_ROW, "Network", netStr, 0, GxEPD_BLACK});

    if (oled_isAvailable()) oled_showToast("Updating EPD...", 1500);
    epd_displayPage(page);
}

static void view_next(void) {
    auto count = BeszelService::getInstance().getSystemCount();
    if (count <= 1) return;
    s_prevIndex = s_index;
    s_index = (s_index + 1) % (uint8_t)count;
    ui_triggerVerticalAnimation(true);
}

static void view_prev(void) {
    auto count = BeszelService::getInstance().getSystemCount();
    if (count <= 1) return;
    s_prevIndex = s_index;
    s_index = (s_index + (uint8_t)count - 1) % (uint8_t)count;
    ui_triggerVerticalAnimation(false);
}

static void view_select(void) {
    const auto& systems = BeszelService::getInstance().getSystems();
    if (s_index < systems.size()) {
        update_epaper(systems[s_index]);
    } else {
        fetch_data();
    }
}

static void view_back(void) {
    ui_setView(NULL);
}

static float view_get_progress(void) {
    auto count = BeszelService::getInstance().getSystemCount();
    if (count == 0) return 0.0f;
    return (float)(s_index + 1) / (float)count;
}

static void view_poll(void) {
    // Optional: auto-fetch in foreground
}

static const View VIEW_BESZEL = {
    .title = "Beszel",
    .render = view_render,
    .onNext = view_next,
    .onPrev = view_prev,
    .onSelect = view_select,
    .onBack = view_back,
    .poll = view_poll,
    .getScrollProgress = view_get_progress
};

static void app_renderPreview(int16_t x, int16_t y) {
    auto count = BeszelService::getInstance().getSystemCount();
    char buf[16];
    snprintf(buf, sizeof(buf), "%zu nodes", count);
    comp_title_and_text("Beszel", count > 0 ? buf : "No data", x, y, false);
}

static void app_select(void) {
    s_index = 0;
    ui_setView(&VIEW_BESZEL);
    if (BeszelService::getInstance().getSystemCount() == 0) {
        fetch_data();
    }
}

static void app_setup(void) {
    BeszelService::getInstance().begin("https://beszel.francesco-bruno.com/");
}

const App APP_BESZEL = {
    .name = "Beszel",
    .renderPreview = app_renderPreview,
    .onSelect = app_select,
    .setup = app_setup,
    .registerRoutes = nullptr,
    .poll = nullptr 
};
