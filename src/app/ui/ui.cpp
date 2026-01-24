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

// Physics constants (Unified for consistent feel)
static constexpr float ANIM_K = 1.0f;   // More speed
static constexpr float ANIM_D = 0.35f;  // Less bounce

// Animation state
static float s_animOffset = 0.0f;  // 0.0 = centered, 1.0 = incoming from bottom, -1.0 = incoming from top
static float s_animVelocity = 0.0f;
static float s_hAnimOffset = 0.0f; // 0.0 = carousel, 1.0 = in-app
static float s_hAnimVelocity = 0.0f;
static float s_hAnimTarget = 0.0f;
static float s_progress = 0.0f; // Smoothed vertical progress
static float s_progressOpacity = 0.0f; // 0.0 to 1.0
static uint32_t s_lastInputTime = 0;
static size_t s_prevAppIndex = 0;
static const View *s_lastView = NULL; // For exit transition

// Internal rendering helper for carousel
// Internal rendering helper for carousel
static void ui_renderAppPreview(size_t index, int16_t x_offset, int16_t y_offset) {
    const App** apps = registry_getApps();
    size_t count = registry_getCount();
    if (index >= count) return;
    
    if (index == 0) {
        // Centralized logic for Home (Clock) to ensure size
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
        oled_drawHomeScreen(timebuf, wifi_isConnected(), x_offset, y_offset, false);
    } else {
        // Centralized logic for other apps (Titles) to ensure size
        oled_drawBigText(apps[index]->name, x_offset, y_offset, false);
    }
}

// Helpers
void ui_redraw(void) {
  // Horizontal translation logic
  int16_t h_px = (int16_t)(s_hAnimOffset * 128.0f);

  oled_clearBuffer();

  // If E-Paper is busy rendering in background, we could show a tiny indicator here
  // but for now let's just allow the UI to remain fully active as requested.

  // Draw Carousel if visible
  if (s_hAnimOffset < 0.99f) {
      int16_t carousel_x = -h_px;
      if (abs(s_animOffset) < 0.01f) {
          ui_renderAppPreview(s_appIndex, carousel_x, 0);
      } else {
          int16_t offset_y_px = (int16_t)(s_animOffset * 64.0f);
          ui_renderAppPreview(s_appIndex, carousel_x, offset_y_px);
          ui_renderAppPreview(s_prevAppIndex, carousel_x, offset_y_px > 0 ? offset_y_px - 64 : offset_y_px + 64);
      }
  }

  // Draw View if visible
  if (s_hAnimOffset > 0.01f) {
      int16_t view_x = 128 - h_px;
      int16_t view_y = (int16_t)(s_animOffset * 64.0f);
      const View* v = s_currentView ? s_currentView : s_lastView;
      if (v) {
          if (v->render) v->render(view_x, view_y);
          if (v->title) oled_drawHeader(v->title, view_x, 0);
      }
  }

  // Draw Vertical Scroll Progress (1px bar on the left)
  if (s_progress > 0.001f && s_progressOpacity > 0.001f) {
      if (s_progressOpacity >= 0.99f) {
          oled_drawScrollProgress(s_progress);
      } else {
          // Subtle fade by drawing dots (dithering style) if low opacity 
          // but for simplicity on OLED we just draw if > 0.5 or similar,
          // OR we just draw it normally if we want it crisp.
          // Let's draw it normally for now as requested "line".
          oled_drawScrollProgress(s_progress);
      }
  }
  
  oled_drawActiveToast();
  oled_display();
}

void ui_setView(const View* view) {
    if (view == s_currentView) return;

    if (view != NULL) {
        // Entering App: target progress 1.0
        s_hAnimTarget = 1.0f;
        s_lastView = NULL;
    } else {
        // Exiting App: target progress 0.0
        s_hAnimTarget = 0.0f;
        s_lastView = s_currentView;
    }

    // Reset vertical animation on view change
    s_animOffset = 0.0f;
    s_animVelocity = 0.0f;

    s_currentView = view;
    ui_redraw();
}

void ui_triggerVerticalAnimation(bool up) {
    s_animOffset = up ? 1.0f : -1.0f;
    s_animVelocity = 0.0f;
    oled_showToast(NULL, 600, up ? TOAST_BOTTOM : TOAST_TOP, up ? TOAST_ICON_DOWN : TOAST_ICON_UP);
    ui_redraw();
}

// Navigation Callbacks
void ui_next(void) {
    s_lastInputTime = millis();
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
        ui_triggerVerticalAnimation(true);
    }
}

void ui_prev(void) {
    s_lastInputTime = millis();
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
        ui_triggerVerticalAnimation(false);
    }
}

void ui_select(void) {
    s_lastInputTime = millis();
    // If inside a view, delegate
    if (s_currentView) {
        if (s_currentView->onSelect) s_currentView->onSelect();
        return;
    }

    // Carousel: Open App
    const App** apps = registry_getApps();
    size_t count = registry_getCount();
    if (s_appIndex < count && apps[s_appIndex]->onSelect) {
        apps[s_appIndex]->onSelect();
    }
}

void ui_back(void) {
    s_lastInputTime = millis();
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

  s_currentView = NULL;
  s_lastView = NULL;
  s_timeConfigured = false;
  s_initialDateShown = false;
  s_animOffset = 0.0f;
  s_animVelocity = 0.0f;
  s_hAnimOffset = 0.0f;
  s_hAnimVelocity = 0.0f;
  s_prevAppIndex = 0;
  s_progress = 0.0f;
  s_progressOpacity = 0.0f;
  s_lastInputTime = 0;

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

  // Handle Carousel Animation (Vertical)
  if (abs(s_animOffset) > 0.001f || abs(s_animVelocity) > 0.001f) {
      float force = -ANIM_K * s_animOffset;
      s_animVelocity += force;
      s_animVelocity *= ANIM_D;
      s_animOffset += s_animVelocity;
      if (abs(s_animOffset) < 0.005f && abs(s_animVelocity) < 0.005f) {
          s_animOffset = 0.0f;
          s_animVelocity = 0.0f;
      }
      ui_redraw();
  }

  // Handle Horizontal Transition (App Enter/Exit)
  if (abs(s_hAnimOffset - s_hAnimTarget) > 0.001f || abs(s_hAnimVelocity) > 0.001f) {
      float force = -ANIM_K * (s_hAnimOffset - s_hAnimTarget);
      s_hAnimVelocity += force;
      s_hAnimVelocity *= ANIM_D;
      s_hAnimOffset += s_hAnimVelocity;

      if (abs(s_hAnimOffset - s_hAnimTarget) < 0.005f && abs(s_hAnimVelocity) < 0.005f) {
          s_hAnimOffset = s_hAnimTarget;
          s_hAnimVelocity = 0.0f;
          if (s_hAnimTarget == 0.0f) s_lastView = NULL;
      }
      ui_redraw();
  }

  // Handle Progress Animation
  float target_p = 0.0f;
  if (s_hAnimOffset > 0.5f) {
      // In-App progress
      const View* v = s_currentView ? s_currentView : s_lastView;
      if (v && v->getScrollProgress) target_p = v->getScrollProgress();
  } else {
      // Carousel progress (Skip Home at index 0)
      size_t count = registry_getCount();
      if (count > 1) {
          if (s_appIndex == 0) target_p = 0.0f;
          else target_p = (float)s_appIndex / (float)(count - 1);
      }
  }

  if (abs(s_progress - target_p) > 0.001f) {
      s_progress += (target_p - s_progress) * 0.4f; // Smooth transition (faster)
      ui_redraw();
  }

  // Handle Scrollbar Visibility (Fades after 1 second)
  float target_opacity = (millis() - s_lastInputTime < 1000) ? 1.0f : 0.0f;
  if (abs(s_progressOpacity - target_opacity) > 0.001f) {
      s_progressOpacity += (target_opacity - s_progressOpacity) * 0.2f;
      if (abs(s_progressOpacity - target_opacity) < 0.01f) s_progressOpacity = target_opacity;
      ui_redraw();
  }

  // Handle Hold Progress (Back action feedback)
  float holdP = controls_getConfirmHoldProgress();
  if (holdP > 0.01f) {
      oled_showHoldToast(TOAST_BOTTOM, TOAST_ICON_BACK, holdP);
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
