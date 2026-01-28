#include "ha_service.h"
#include "app/ui/ui_internal.h"
#include "app/ui/common/types.h"
#include "app/ui/common/components.h"
#include "drivers/oled/oled.h"
#include "drivers/epaper/display.h"
#include "drivers/epaper/layout.h"
#include <GxEPD2_3C.h>
#include "secrets.h"

// State
static uint8_t s_index = 0;
static uint8_t s_prevIndex = 0;

// Enum for current mode
enum ListMode {
    MODE_MENU,
    MODE_ACTIVE,
    MODE_COMPLETED
};

static ListMode s_mode = MODE_MENU;

// Helper to get current list based on mode
static const std::vector<HAShoppingItem>& getCurrentList() {
    if (s_mode == MODE_ACTIVE) return HAService::getInstance().getActiveItems();
    return HAService::getInstance().getCompletedItems();
}

static void update_epaper();

static void fetch_data(bool update_epd = false) {
    if (oled_isAvailable()) oled_showToast("Syncing List...", 1000);
    if (HAService::getInstance().fetchList()) {
        if (oled_isAvailable()) oled_showToast("List Updated", 800);
        if (update_epd) update_epaper();
    } else {
        if (oled_isAvailable()) oled_showToast("Sync Failed", 1500);
    }
}

// --- Menu View ---
static void menu_render(int16_t x, int16_t y) {
    // Simple 2-item menu
    const char* title = "Show List";
    const char* opt1 = "1. Active";
    const char* opt2 = "2. Completed";
    
    // Draw based on s_index (0=Active, 1=Completed)
    if (y == 0) {
        oled_drawScrollingText(s_index == 0 ? opt1 : opt2, x, 0, false);
    } else {
         // Not scrolling detailed menu, just toggle
         if (abs(y) < 1) {
             oled_drawScrollingText(s_index == 0 ? opt1 : opt2, x, 0, false);
         } else {
             // prev/next
             oled_drawScrollingText(s_index == 0 ? opt2 : opt1, x, y, false);
         }
    }
}

static void menu_next() {
    s_index = (s_index + 1) % 2;
    ui_triggerVerticalAnimation(true);
}
static void menu_prev() {
    s_index = (s_index + 1) % 2; // 2 items, same logic
    ui_triggerVerticalAnimation(false);
}

// Forward Decl
// Forward Decl
extern const View VIEW_HA_LIST;

static void menu_select() {
    s_mode = (s_index == 0) ? MODE_ACTIVE : MODE_COMPLETED;
    s_index = 0; // Reset for list
    ui_setView(&VIEW_HA_LIST);
    update_epaper(); // Update EPD with new list
}

static void menu_back() {
    ui_setView(NULL);
}

static const View VIEW_HA_MENU = {
    .title = "Shopping",
    .render = menu_render,
    .onNext = menu_next,
    .onPrev = menu_prev,
    .onSelect = menu_select,
    .onBack = menu_back,
    .poll = nullptr,
    .getScrollProgress = nullptr
};

// --- List View ---

static void render_item(uint8_t index, int16_t x, int16_t y) {
    const auto& items = getCurrentList();
    if (items.empty()) {
        oled_drawBigText("Empty List", x, y, false);
        return;
    }
    
    if (index < items.size()) {
        const auto& item = items[index];
        char buf[64];
        snprintf(buf, sizeof(buf), "%d. %s", index + 1, item.name.c_str());
        oled_drawScrollingText(buf, x, y, false);
    }
}

static void view_render(int16_t x_offset, int16_t y_offset) {
    auto count = getCurrentList().size();
    if (count == 0) {
        oled_drawBigText("No Items", x_offset, y_offset, false);
        return;
    }

    if (abs(y_offset) < 1) {
        render_item(s_index, x_offset, 0);
    } else {
        render_item(s_index, x_offset, y_offset);
        render_item(s_prevIndex, x_offset, y_offset > 0 ? y_offset - 64 : y_offset + 64);
    }
}

static void update_epaper() {
    // Remove busy check to allow queuing
    // if (epd_isBusy()) return; 

    // Show BOTH lists: Active first, then Completed
    const auto& active = HAService::getInstance().getActiveItems();
    const auto& completed = HAService::getInstance().getCompletedItems();
    
    EpdPage page;
    page.title = "Shopping List";
    
    if (active.empty() && completed.empty()) {
        page.components.push_back({EPD_COMP_ROW, "List is empty", "", 0, GxEPD_BLACK});
    } else {
        // 1. Active Items
        for (const auto& item : active) {
            String text = "[ ] " + item.name;
            page.components.push_back({EPD_COMP_ROW, text, "", 0, GxEPD_BLACK});
        }
        
        // Separator if we have both
        if (!active.empty() && !completed.empty()) {
             page.components.push_back({EPD_COMP_SEPARATOR, "", "", 0, 0});
        }

        // 2. Completed Items
        for (const auto& item : completed) {
            String text = "[x] " + item.name;
            page.components.push_back({EPD_COMP_ROW, text, "", 0, GxEPD_BLACK});
        }
    }
    
    // Footer removed as per request
    // page.components.push_back({EPD_COMP_SEPARATOR, "", "", 0, 0});
    // size_t total = active.size() + completed.size();
    // page.components.push_back({EPD_COMP_ROW, "Total Items", String(total), 0, GxEPD_BLACK});

    if (oled_isAvailable()) oled_showToast("Updating EPD...", 1500);
    epd_displayPage(page);
}

static void view_next(void) {
    auto count = getCurrentList().size();
    if (count <= 1) return;
    s_prevIndex = s_index;
    s_index = (s_index + 1) % (uint8_t)count;
    ui_triggerVerticalAnimation(true);
}

static void view_prev(void) {
    auto count = getCurrentList().size();
    if (count <= 1) return;
    s_prevIndex = s_index;
    s_index = (s_index + (uint8_t)count - 1) % (uint8_t)count;
    ui_triggerVerticalAnimation(false);
}

static void view_select(void) {
    const auto& items = getCurrentList();
    if (s_index < items.size()) {
        const auto& item = items[s_index];
        bool targetState = (s_mode == MODE_ACTIVE); // If Active, we want to complete (true). If Completed, uncomplete (false).
        
        if (oled_isAvailable()) oled_showToast(targetState ? "Completing..." : "Restoring...", 1000);
        
        if (HAService::getInstance().setComplete(item.id, targetState)) {
             // Success
            if (s_index >= items.size() - 1 && s_index > 0) {
                s_index--;
            }
            fetch_data(true); // Refetch and update EPD
        } else {
            if (oled_isAvailable()) oled_showToast("Failed", 1000);
        }
    } else {
        fetch_data(true); // Refetch
    }
}

static void view_back(void) {
    // Back returns to Menu
    s_index = 0;
    ui_setView(&VIEW_HA_MENU);
}

static float view_get_progress(void) {
    auto count = getCurrentList().size();
    if (count == 0) return 0.0f;
    return (float)(s_index + 1) / (float)count;
}

const View VIEW_HA_LIST = {
    .title = "Shopping",
    .render = view_render,
    .onNext = view_next,
    .onPrev = view_prev,
    .onSelect = view_select,
    .onBack = view_back,
    .poll = nullptr,
    .getScrollProgress = view_get_progress
};

static void app_renderPreview(int16_t x, int16_t y) {
    auto count = HAService::getInstance().getActiveItems().size();
    char buf[16];
    snprintf(buf, sizeof(buf), "%zu items", count);
    comp_title_and_text("Shop List", count > 0 ? buf : "Sync", x, y, false);
}

static void app_select(void) {
    // Start with Sync and Update EPD
    fetch_data(true);
    // Go to Menu
    s_index = 0;
    ui_setView(&VIEW_HA_MENU);
}

static void app_setup(void) {
    HAService::getInstance().begin(HA_URL, HA_TOKEN);
}

extern const App APP_HA_LIST = {
    .name = "Shopping",
    .renderPreview = app_renderPreview,
    .onSelect = app_select,
    .setup = app_setup,
    .registerRoutes = nullptr,
    .poll = nullptr 
};
