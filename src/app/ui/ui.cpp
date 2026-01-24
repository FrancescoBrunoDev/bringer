/*
 * ui.cpp
 *
 * Refactored UI using a registry of apps.
 * Supports a Carousel of Apps and entering them.
 */

#include "ui.h"
#include "ui_internal.h"
#include "registry.h"
#include "drivers/oled/oled.h"
#include "app/controls/controls.h"
#include "app/wifi/wifi.h"
#include "drivers/epaper/display.h"

#include <Arduino.h>
#include <time.h>

// Current state
static size_t s_appIndex = 0;           // Index in the app registry (Carousel)
static const View *s_currentView = NULL; // If non-NULL, we are "inside" an app loop
static bool s_timeConfigured = false;
static bool s_initialDateShown = false;

// Animation state
static float s_animOffset = 0.0f;  // 0.0 = centered, 1.0 = incoming from bottom, -1.0 = incoming from top
static size_t s_prevAppIndex = 0;

// Internal rendering helper for carousel
static void ui_renderAppPreview(size_t index, int16_t y_offset) {
    const App** apps = registry_getApps();
    size_t count = registry_getCount();
    if (index >= count) return;

    if (index == 0) {
        // Home app shows clock. 
        // We reuse the logic from components.cpp but with offset support.
        char timebuf[16];
        time_t now = time(nullptr);
        if (now > 1600000000) {
            struct tm tm;
            localtime_r(&now, &tm);
            strftime(timebuf, sizeof(timebuf), "%H:%M", &tm);
        } else {
            unsigned long s = millis() / 1000;
            snprintf(timebuf, sizeof(timebuf), "%02lu:%02lu", (s/3600)%100, (s/60)%60);
        }
        oled_drawHomeScreen(timebuf, wifi_isConnected(), y_offset, false);
    } else {
        // Other apps show their name large and centered
        oled_drawBigText(apps[index]->name, y_offset, false);
    }
}

// Helpers
void ui_redraw(void) {
  if (epd_isBusy()) {
    if (oled_isAvailable()) {
      oled_showLines("EPD", "Updating...");
    } else {
      Serial.println("UI: EPD busy (updating)");
    }
    return;
  }

  // If we are inside an app's specific view, render it
  if (s_currentView && s_currentView->render) {
    s_currentView->render();
    return;
  }

  // Otherwise, render the Carousel with animation
  oled_clearBuffer();
  
  if (abs(s_animOffset) < 0.01f) {
      // Static view
      ui_renderAppPreview(s_appIndex, 0);
  } else {
      // Dynamic view (scrolling)
      int16_t offset_px = (int16_t)(s_animOffset * 64.0f);
      
      // Moving NEXT (suggests scrolling UP, new app comes from bottom)
      // If s_animOffset > 0, we are transitioning from s_prevAppIndex to s_appIndex.
      // Current s_appIndex is at offset_px (starts at 64, goes to 0)
      // Previous s_prevAppIndex is at offset_px - 64 (starts at 0, goes to -64)
      
      ui_renderAppPreview(s_appIndex, offset_px);
      ui_renderAppPreview(s_prevAppIndex, offset_px > 0 ? offset_px - 64 : offset_px + 64);
  }
  
  oled_drawActiveToast();
  oled_display();
}

void ui_setView(const View* view) {
    s_currentView = view;
    ui_redraw();
}

// Navigation Callbacks
void ui_next(void) {
    // If inside a view, delegate
    if (s_currentView) {
        if (s_currentView->onNext) s_currentView->onNext();
        return;
    }

    // Carousel navigation
    size_t count = registry_getCount();
    if (count > 0) {
        s_prevAppIndex = s_appIndex;
        s_appIndex = (s_appIndex + 1) % count;
        s_animOffset = 1.0f; // Incoming from bottom
        oled_showToast(NULL, 600, TOAST_BOTTOM, TOAST_ICON_DOWN);
        ui_redraw();
    }
}

void ui_prev(void) {
    // If inside a view, delegate
    if (s_currentView) {
        if (s_currentView->onPrev) s_currentView->onPrev();
        return;
    }

    // Carousel navigation
    size_t count = registry_getCount();
    if (count > 0) {
        s_prevAppIndex = s_appIndex;
        s_appIndex = (s_appIndex + count - 1) % count;
        s_animOffset = -1.0f; // Incoming from top
        oled_showToast(NULL, 600, TOAST_TOP, TOAST_ICON_UP);
        ui_redraw();
    }
}

void ui_select(void) {
    // If inside a view, delegate
    if (s_currentView) {
        if (s_currentView->onSelect) s_currentView->onSelect();
        return;
    }

    // Carousel: Open App
    const App** apps = registry_getApps();
    size_t count = registry_getCount();
    if (s_appIndex < count && apps[s_appIndex]->onSelect) {
        oled_showToast("Selected", 800, TOAST_BOTTOM, TOAST_ICON_SELECT);
        apps[s_appIndex]->onSelect();
    }
}

void ui_back(void) {
    // If inside a view, delegate
    if (s_currentView) {
        if (s_currentView->onBack) {
            s_currentView->onBack();
            return;
        }
        // Default back: exit to carousel
        s_currentView = NULL;
        ui_redraw();
        return;
    }

    // Carousel: Reset to Home (App 0) if not already there
    if (s_appIndex != 0) {
        s_appIndex = 0;
        ui_redraw();
    }
}

// Initialization and Polling
void ui_init(void) {
  controls_setUseDefaultActions(false);
  controls_setPrevCallback(ui_prev);
  controls_setNextCallback(ui_next);
  controls_setConfirmCallback(ui_select);
  controls_setConfirmLongCallback(ui_back);

  s_appIndex = 0;
  s_currentView = NULL;
  s_timeConfigured = false;
  s_initialDateShown = false;
  s_animOffset = 0.0f;
  s_prevAppIndex = 0;

  oled_setMenuMode(true);
  ui_redraw();
}

void ui_poll(void) {
  // NTP Sync Logic
  // NTP Sync Logic
  if (wifi_isConnected() && !s_timeConfigured) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    s_timeConfigured = true;
  }

  // Once time is valid, show date on E-Paper (once)
  if (s_timeConfigured && !s_initialDateShown) {
      time_t now = time(nullptr);
      if (now > 1600000000) {
          epd_displayDate(now);
          s_initialDateShown = true;
      }
  }

  // Handle Carousel Animation
  if (abs(s_animOffset) > 0.001f) {
      s_animOffset *= 0.6f; // Smooth decay
      if (abs(s_animOffset) < 0.05f) s_animOffset = 0.0f;
      ui_redraw();
  }

  if (oled_poll()) {
    ui_redraw();
    return;
  }

  // Poll current View or App
  if (s_currentView) {
      if (s_currentView->poll) s_currentView->poll();
  } else {
      const App** apps = registry_getApps();
      size_t count = registry_getCount();
      if (s_appIndex < count && apps[s_appIndex]->poll) {
          apps[s_appIndex]->poll();
      }
  }
}

// Introspection for Web UI
int ui_getState(void) {
    // Try to map to old behavior for compatibility
    // 0: Home, 1: Text, 2: Settings Overview, 3: Settings Menu
    if (s_appIndex == 0) return 0;
    if (s_appIndex == 1) return 1; 
    if (s_appIndex == 2) {
        if (s_currentView == NULL) return 2;
        return 3;
    }
    return (int)s_appIndex; 
}

int ui_getIndex(void) {
    // If inside text app/settings, maybe return internal index?
    // Since we don't expose internal app state easily, 
    // we return s_appIndex or 0.
    // The old code returned s_index which was shared.
    // This might be a breaking change for introspection if it relied on exact internal index values.
    // For now, return s_appIndex (Carousel index).
    return (int)s_appIndex;
}

bool ui_isInApp(void) {
    // Old behavior: strictly when viewing text menu
    // New behavior approximation:
    return (s_appIndex == 1 && s_currentView != NULL);
}
