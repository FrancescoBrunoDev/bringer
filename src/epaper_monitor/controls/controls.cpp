/*
 * controls.cpp
 *
 * Implementation of a small, robust two-button handler (debounce + optional long-press)
 * - Default actions:
 *     Clear button -> epd_clear()
 *     Toggle button -> epd_setPartialEnabled(!epd_getPartialEnabled())
 * - Custom callbacks can be registered for both short press and a single long-press callback
 *
 * Buttons are expected to be wired active-low: button connects the GPIO to GND when pressed.
 */

#include "epaper_monitor/controls/controls.h"
#include "epaper_monitor/display/display.h"
#include "epaper_monitor/oled/oled.h"

#include <Arduino.h>

// Internal state for a single button
struct ButtonState {
  uint8_t pin = 0;
  int raw = HIGH;               // last raw reading
  int stable = HIGH;            // last stable (debounced) state
  int idleState = HIGH;         // detected idle state (helps support active-high or active-low wiring)
  unsigned long lastChange = 0; // last time raw reading changed
  unsigned long pressStart = 0; // time when stable went LOW
  bool longFired = false;       // whether long-press callback already fired for this press
};

static ButtonState s_clearBtn;
static ButtonState s_toggleBtn;

static unsigned long s_debounceMs = 50;
static unsigned long s_longPressMs = 1000;

static controls_button_cb_t s_clear_cb = nullptr;
static controls_button_cb_t s_toggle_cb = nullptr;
static controls_button_cb_t s_longpress_cb = nullptr;
static controls_button_cb_t s_clear_long_cb = nullptr;
static controls_button_cb_t s_toggle_long_cb = nullptr;

static bool s_useDefaultActions = true;

// Default actions (invoked if no custom callback is set and defaults are enabled)
static void defaultClearAction() {
  epd_clear();
}
static void defaultToggleAction() {
  bool cur = epd_getPartialEnabled();
  epd_setPartialEnabled(!cur);
}
static void defaultLongPressAction() {
  // Run the recovery clear sequence (long operation)
  if (!epd_forceClear_async()) {
    // If a job is already running, briefly indicate busy state on the OLED
    if (oled_isAvailable()) oled_showStatus("EPD busy");
  }
}

/* ---------- Initialization ---------- */
void controls_init(uint8_t clearPin, uint8_t togglePin, unsigned long debounceMs) {
  s_clearBtn.pin = clearPin;
  s_toggleBtn.pin = togglePin;
  s_debounceMs = debounceMs;

  Serial.printf("controls_init: clearPin=%d togglePin=%d debounceMs=%lu\n", clearPin, togglePin, s_debounceMs);

  // Configure pins. Your buttons are wired to GND (active-low), so enable
  // the internal pull-ups to keep the pins HIGH at idle and read LOW when
  // the button is pressed.
  pinMode(s_clearBtn.pin, INPUT_PULLUP);
  pinMode(s_toggleBtn.pin, INPUT_PULLUP);

  // Initial readings
  s_clearBtn.raw = digitalRead(s_clearBtn.pin);
  s_clearBtn.stable = s_clearBtn.raw;
  s_clearBtn.idleState = s_clearBtn.stable;
  s_clearBtn.lastChange = millis();
  s_clearBtn.pressStart = 0;
  s_clearBtn.longFired = false;

  s_toggleBtn.raw = digitalRead(s_toggleBtn.pin);
  s_toggleBtn.stable = s_toggleBtn.raw;
  s_toggleBtn.idleState = s_toggleBtn.stable;
  s_toggleBtn.lastChange = millis();
  s_toggleBtn.pressStart = 0;
  s_toggleBtn.longFired = false;

  Serial.printf("controls_init: initial raw clear=%d stable=%d toggle raw=%d stable=%d\n",
                s_clearBtn.raw, s_clearBtn.stable, s_toggleBtn.raw, s_toggleBtn.stable);

  // If defaults enabled and no callbacks provided, leave callbacks null:
  // callbacks are checked at invocation-time and fall back to defaults if needed.
}

/* ---------- Callbacks registration ---------- */
void controls_setClearCallback(controls_button_cb_t cb) {
  s_clear_cb = cb;
}

void controls_setToggleCallback(controls_button_cb_t cb) {
  s_toggle_cb = cb;
}

void controls_setLongPressCallback(controls_button_cb_t cb) {
  s_longpress_cb = cb;
}

void controls_setClearLongCallback(controls_button_cb_t cb) {
  s_clear_long_cb = cb;
}

void controls_setToggleLongCallback(controls_button_cb_t cb) {
  s_toggle_long_cb = cb;
}

void controls_setLongPressMs(unsigned long ms) {
  s_longPressMs = ms;
}

void controls_setUseDefaultActions(bool enable) {
  s_useDefaultActions = enable;
}

// Diagnostic helpers
uint8_t controls_getClearPin(void) { return s_clearBtn.pin; }
uint8_t controls_getTogglePin(void) { return s_toggleBtn.pin; }
int controls_readPin(uint8_t pin) { return digitalRead(pin); }

/* ---------- Polling + debounce logic ---------- */
// Helper that handles a single button's state machine.
// - b: reference to ButtonState
// - onShort: short-press handler (may be null)
// - onLong: long-press handler (may be null)
static void _pollButton(ButtonState &b, controls_button_cb_t onShort, controls_button_cb_t onLong) {
  int reading = digitalRead(b.pin);

  // Raw change detection
  if (reading != b.raw) {
    b.lastChange = millis();
    b.raw = reading;
    Serial.printf("controls: pin %d raw changed -> %d\n", b.pin, b.raw);
    // If the device has an attached USB-serial chip that resets on DTR/RTS,
    // rapid connect/disconnects can be observed by platformio monitor when
    // pressing a pin wired to the USB-serial control lines. Log an extra
    // message to help correlate disconnects.
    Serial.printf("controls-debug: pin %d lastChange=%lu\n", b.pin, b.lastChange);
  }

  // If the raw reading has been stable for more than debounce period,
  // and differs from previously stable state, commit the new stable state.
  if ((millis() - b.lastChange) > s_debounceMs) {
    if (b.raw != b.stable) {
      b.stable = b.raw;
      if (b.stable != b.idleState) {
        // Button became pressed: mark press start
        b.pressStart = millis();
        b.longFired = false;
        Serial.printf("controls: pin %d pressed\n", b.pin);
      } else {
        // Button released: if long press wasn't fired, treat as short press
        if (!b.longFired) {
          if (onShort) {
            Serial.printf("controls: pin %d short -> custom callback\n", b.pin);
            onShort();
          } else if (s_useDefaultActions) {
            Serial.printf("controls: pin %d short -> default action\n", b.pin);
            // choose the correct default depending on which button
            if (&b == &s_clearBtn) defaultClearAction();
            else if (&b == &s_toggleBtn) defaultToggleAction();
          } else {
            Serial.printf("controls: pin %d short -> no action registered\n", b.pin);
          }
        }
        // reset pressStart
        b.pressStart = 0;
        b.longFired = false;
      }
    } else {
      // If button is held down, check for long press
      if (b.stable != b.idleState && !b.longFired && b.pressStart != 0) {
        if ((millis() - b.pressStart) >= s_longPressMs) {
          // long press detected
          if (onLong) {
            Serial.printf("controls: pin %d long -> custom per-button callback\n", b.pin);
            onLong();
          } else if (s_longpress_cb) {
            Serial.printf("controls: pin %d long -> global long callback\n", b.pin);
            s_longpress_cb();
          } else if (s_useDefaultActions) {
            Serial.printf("controls: pin %d long -> default long action\n", b.pin);
            defaultLongPressAction();
          } else {
            Serial.printf("controls: pin %d long -> no action registered\n", b.pin);
          }

          b.longFired = true;
        }
      }
    }
  }
}

void controls_poll(void) {
  // Must be called frequently (e.g., from loop())
  // Prefer per-button long-press callbacks when set; otherwise fall back to the global one.
  _pollButton(s_clearBtn, s_clear_cb, (s_clear_long_cb ? s_clear_long_cb : s_longpress_cb));
  _pollButton(s_toggleBtn, s_toggle_cb, (s_toggle_long_cb ? s_toggle_long_cb : s_longpress_cb));
}
