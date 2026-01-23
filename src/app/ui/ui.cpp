
/*
 * ui.cpp
 *
 * Simple menu-driven UI that leverages the SSD1306 OLED for quick navigation.
 *
 * Controls:
 *  - Button A (Clear)  : scroll between menu items (short press)
 *  - Button B (Toggle) : select current item (short press)
 *  - Button B (Toggle) : go back / exit menu (long press)
 *
 * Initial screen: "Settings" with two submenu items:
 *  - Partial update: ON/OFF   (toggles runtime partial updates)
 *  - Full cleaning            (runs recovery full clear on the e-paper)
 *
 * The module registers callbacks on the controls module and uses the oled & display APIs.
 */

#include "ui.h"
#include "drivers/oled/oled.h"
#include "app/controls/controls.h"
#include "drivers/epaper/display.h"
#include "app/wifi/wifi.h"
#include "app/routes/text_app/text_app.h"
#include <GxEPD2_3C.h>

#include <Arduino.h>
#include <stdio.h>
#include <time.h>

// (no toasts) forward declarations not needed

// Settings submenu items
enum SettingsItem : uint8_t { SET_IP = 0, SET_PARTIAL, SET_FULL_CLEAN, SET_COUNT };

// Views and components are implemented in separate files to keep UI maintainable.
#include "components.h"
#include "views.h"

// Forward view handlers and renderers (small, kept local)
static void view_home_render(void);
static void view_home_next(void);
static void view_placeholder_render(void);
static void view_placeholder_next(void);
static void view_placeholder_select(void);
static void view_text_menu_render(void);
static void view_text_menu_next(void);
static void view_text_menu_select(void);
static void view_text_menu_back(void);
static void view_settings_overview_render(void);
static void view_settings_overview_next(void);
static void view_settings_overview_select(void);
static void view_settings_menu_render(void);
static void view_settings_menu_next(void);
static void view_settings_menu_select(void);
static void view_settings_menu_back(void);

// Define the views (externs declared in views.h)
const View VIEW_HOME = { view_home_render, view_home_next, NULL, NULL, NULL };
const View VIEW_PLACEHOLDER = { view_placeholder_render, view_placeholder_next, view_placeholder_select, NULL, NULL };
const View VIEW_TEXT_MENU = { view_text_menu_render, view_text_menu_next, view_text_menu_select, view_text_menu_back, NULL };
const View VIEW_SETTINGS_OVERVIEW = { view_settings_overview_render, view_settings_overview_next, view_settings_overview_select, NULL, NULL };
const View VIEW_SETTINGS_MENU = { view_settings_menu_render, view_settings_menu_next, view_settings_menu_select, view_settings_menu_back, NULL };

// Current view pointer and minimal shared state
static const View *s_currentView = &VIEW_HOME;
// index used by settings menu
uint8_t s_index = 0;
static bool s_timeConfigured = false;
static unsigned long s_lastHomeUpdate = 0;

// Central redraw helper: shows EPD busy indicator or delegates to current view
static void ui_redraw(void) {
  if (epd_isBusy()) {
    if (oled_isAvailable()) {
      oled_showLines("EPD", "Updating...");
    } else {
      Serial.println("UI: EPD busy (updating)");
    }
    return;
  }
  if (s_currentView && s_currentView->render) s_currentView->render();
}

// View implementations (small wrappers that use components)
static void view_home_render(void) { comp_time_and_wifi(); }
static void view_home_next(void) { s_currentView = &VIEW_PLACEHOLDER; s_index = 0; ui_redraw(); }

static void view_placeholder_render(void) {
  // Overview: show app title only. Select will enter the app menu.
  comp_title_and_text("Text App", "");
}

static void view_placeholder_next(void) {
  // Move to Settings overview (follow original menu order)
  s_currentView = &VIEW_SETTINGS_OVERVIEW;
  s_index = 0;
  ui_redraw();
}

static void view_placeholder_select(void) {
  // Enter the Text App menu (nested view)
  s_currentView = &VIEW_TEXT_MENU;
  s_index = 0;
  ui_redraw();
}

static void view_text_menu_render(void) {
  const char *txt = text_app_get_text(s_index);
  if (txt) {
    comp_title_and_text("Text App", txt);
  } else {
    comp_title_and_text("Text App", "(no options)");
  }
}

static void view_text_menu_next(void) {
  size_t count = text_app_get_count();
  if (count == 0) {
    ui_redraw();
    return;
  }
  s_index = (s_index + 1) % (uint8_t)count;
  ui_redraw();
}

static void view_text_menu_select(void) {
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

  ui_redraw();
}

static void view_text_menu_back(void) {
  // Go back to overview (title-only) screen
  s_currentView = &VIEW_PLACEHOLDER;
  s_index = 0;
  ui_redraw();
}

static void view_settings_overview_render(void) { comp_title_and_text("Settings", ""); }
static void view_settings_overview_next(void) { s_currentView = &VIEW_HOME; s_index = 0; ui_redraw(); }
static void view_settings_overview_select(void) { s_currentView = &VIEW_SETTINGS_MENU; s_index = 0; ui_redraw(); }

static void view_settings_menu_render(void) {
  char buf[40];
  switch (s_index) {
    case SET_IP: {
      IPAddress ip = wifi_getIP();
      if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
        comp_title_and_text("Settings", "IP: none");
      } else {
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        comp_title_and_text("Settings", buf);
      }
      break;
    }
    case SET_PARTIAL:
      snprintf(buf, sizeof(buf), "Partial: %s", epd_getPartialEnabled() ? "ON" : "OFF");
      comp_title_and_text("Settings", buf);
      break;
    case SET_FULL_CLEAN:
      comp_title_and_text("Settings", "Full cleaning");
      break;
    default:
      comp_title_and_text("Settings", "");
      break;
  }
}

static void view_settings_menu_next(void) { s_index = (s_index + 1) % SET_COUNT; ui_redraw(); }

static void view_settings_menu_select(void) {
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

static void view_settings_menu_back(void) { s_currentView = &VIEW_SETTINGS_OVERVIEW; s_index = 0; ui_redraw(); }

/* Public API (registered as button callbacks) */
void ui_init(void) {
  // Disable default actions from controls (we handle button behavior here)
  controls_setUseDefaultActions(false);

  // Register navigation callbacks:
  controls_setClearCallback(ui_next);
  controls_setToggleCallback(ui_select);
  controls_setToggleLongCallback(ui_back);

  // Start on the home view
  s_index = 0;
  s_currentView = &VIEW_HOME;
  s_timeConfigured = false;
  s_lastHomeUpdate = 0;

  //'oro Reserve the OLED for the menu UI so e-paper status/progress are suppressed.
  oled_setMenuMode(true);

  // Draw home screen initially
  ui_redraw();
}

void ui_next(void) {
  // Delegate to current view's onNext handler if present
  if (s_currentView && s_currentView->onNext) {
    s_currentView->onNext();
    return;
  }
}

void ui_select(void) {
  // Delegate to current view's onSelect handler if present
  if (s_currentView && s_currentView->onSelect) {
    s_currentView->onSelect();
    return;
  }
}

void ui_back(void) {
  // Delegate to current view's onBack handler if present; fallback to home
  if (s_currentView && s_currentView->onBack) {
    s_currentView->onBack();
    return;
  }
  // Default fallback: go to home view
  s_currentView = &VIEW_HOME;
  s_index = 0;
  if (s_currentView && s_currentView->render) s_currentView->render();
}

int ui_getState(void) {
  if (s_currentView == &VIEW_HOME) return 0;
  if (s_currentView == &VIEW_PLACEHOLDER || s_currentView == &VIEW_TEXT_MENU) return 1;
  if (s_currentView == &VIEW_SETTINGS_OVERVIEW) return 2;
  if (s_currentView == &VIEW_SETTINGS_MENU) return 3;
  return -1;
}
int ui_getIndex(void) { return (int)s_index; }
bool ui_isInApp(void) { return s_currentView == &VIEW_TEXT_MENU; }

// Periodic UI poll: updates home screen clock and handles NTP init when WiFi connects.
// Call this frequently from the main loop.
void ui_poll(void) {
  unsigned long now = millis();
  if ((now - s_lastHomeUpdate) < 1000) return; // update at most once per second
  s_lastHomeUpdate = now;

  // If WiFi is available and we haven't configured NTP yet, do it once.
  if (wifi_isConnected() && !s_timeConfigured) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    s_timeConfigured = true;
  }

  // Check for toast expiry and redraw UI if needed
  if (oled_poll()) {
    // If a toast expired, redraw current screen
    ui_redraw();
    return;
  }

  // If we're showing the home view, refresh it (updates the clock).
  if (s_currentView == &VIEW_HOME) {
    // throttle updates
    unsigned long now = millis();
    if ((now - s_lastHomeUpdate) >= 1000) {
      s_lastHomeUpdate = now;
      if (s_currentView && s_currentView->render) s_currentView->render();
    }
  }
}
