/*
 * controls.cpp
 *
 * Implementation of a small, robust three-button handler (debounce + optional long-press)
 * - Buttons:
 *     Prev    -> typically navigate to previous item (short press)
 *     Next    -> typically navigate to next item (short press)
 *     Confirm -> select/confirm (short press) or cancel/back (long press)
 * - Default actions are preserved for compatibility, but the UI module usually disables them.
 *
 * Buttons are expected to be wired active-low: button connects the GPIO to GND when pressed.
 */

#include "controls.h"
#include "drivers/epaper/display.h"
#include "drivers/oled/oled.h"

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

static ButtonState s_prevBtn;
static ButtonState s_nextBtn;
static ButtonState s_confirmBtn;

static unsigned long s_debounceMs = 50;
static unsigned long s_longPressMs = 1000;

static controls_button_cb_t s_prev_cb = nullptr;
static controls_button_cb_t s_next_cb = nullptr;
static controls_button_cb_t s_confirm_cb = nullptr;
static controls_button_cb_t s_longpress_cb = nullptr;
static controls_button_cb_t s_prev_long_cb = nullptr;
static controls_button_cb_t s_next_long_cb = nullptr;
static controls_button_cb_t s_confirm_long_cb = nullptr;

static bool s_useDefaultActions = true;

// Default actions (invoked if no custom callback is set and defaults are enabled)
static void defaultPrevAction() {
  // Historically 'Clear' cleared the EPD; keep that behavior as a default.
  epd_clear();
}
static void defaultNextAction() {
  // Historically 'Toggle' flipped partial update setting; keep that behavior as a default.
  bool cur = epd_getPartialEnabled();
  epd_setPartialEnabled(!cur);
}
static void defaultConfirmAction() {
  // No-op by default (log for diagnostics)
  Serial.println("controls: defaultConfirmAction -> no action");
}
static void defaultLongPressAction() {
  // Run the recovery clear sequence (long operation)
  if (!epd_forceClear_async()) {
    // If a job is already running, briefly indicate busy state on the OLED
    if (oled_isAvailable()) oled_showStatus("EPD busy");
  }
}

/* ---------- Initialization ---------- */
void controls_init(uint8_t prevPin, uint8_t nextPin, uint8_t confirmPin, unsigned long debounceMs) {
  s_prevBtn.pin = prevPin;
  s_nextBtn.pin = nextPin;
  s_confirmBtn.pin = confirmPin;
  s_debounceMs = debounceMs;

  Serial.printf("controls_init: prevPin=%d nextPin=%d confirmPin=%d debounceMs=%lu\n", prevPin, nextPin, confirmPin, s_debounceMs);

  // Configure pins. Your buttons are wired to GND (active-low), so enable
  // the internal pull-ups to keep the pins HIGH at idle and read LOW when
  // the button is pressed.
  pinMode(s_prevBtn.pin, INPUT_PULLUP);
  pinMode(s_nextBtn.pin, INPUT_PULLUP);
  pinMode(s_confirmBtn.pin, INPUT_PULLUP);

  // Initial readings for all buttons
  s_prevBtn.raw = digitalRead(s_prevBtn.pin);
  s_prevBtn.stable = s_prevBtn.raw;
  // Enforce idle state as HIGH for INPUT_PULLUP (Active Low)
  // This avoids issues if the button is held during boot or pin floats low temporarily
  s_prevBtn.idleState = HIGH; 
  s_prevBtn.lastChange = millis();
  s_prevBtn.pressStart = 0;
  s_prevBtn.longFired = false;

  s_nextBtn.raw = digitalRead(s_nextBtn.pin);
  s_nextBtn.stable = s_nextBtn.raw;
  s_nextBtn.idleState = HIGH; 
  s_nextBtn.lastChange = millis();
  s_nextBtn.pressStart = 0;
  s_nextBtn.longFired = false;

  s_confirmBtn.raw = digitalRead(s_confirmBtn.pin);
  s_confirmBtn.stable = s_confirmBtn.raw;
  s_confirmBtn.idleState = HIGH; 
  s_confirmBtn.lastChange = millis();
  s_confirmBtn.pressStart = 0;
  s_confirmBtn.longFired = false;

  Serial.printf("controls_init: initial raw prev=%d stable=%d next raw=%d stable=%d confirm raw=%d stable=%d\n",
                s_prevBtn.raw, s_prevBtn.stable, s_nextBtn.raw, s_nextBtn.stable, s_confirmBtn.raw, s_confirmBtn.stable);

  // If defaults enabled and no callbacks provided, leave callbacks null:
  // callbacks are checked at invocation-time and fall back to defaults if needed.
}

/* ---------- Callbacks registration ---------- */
void controls_setPrevCallback(controls_button_cb_t cb) {
  s_prev_cb = cb;
}

void controls_setNextCallback(controls_button_cb_t cb) {
  s_next_cb = cb;
}

void controls_setConfirmCallback(controls_button_cb_t cb) {
  s_confirm_cb = cb;
}

void controls_setLongPressCallback(controls_button_cb_t cb) {
  s_longpress_cb = cb;
}

void controls_setPrevLongCallback(controls_button_cb_t cb) {
  s_prev_long_cb = cb;
}

void controls_setNextLongCallback(controls_button_cb_t cb) {
  s_next_long_cb = cb;
}

void controls_setConfirmLongCallback(controls_button_cb_t cb) {
  s_confirm_long_cb = cb;
}

void controls_setLongPressMs(unsigned long ms) {
  s_longPressMs = ms;
}

void controls_setUseDefaultActions(bool enable) {
  s_useDefaultActions = enable;
}

// Diagnostic helpers
uint8_t controls_getPrevPin(void) { return s_prevBtn.pin; }
uint8_t controls_getNextPin(void) { return s_nextBtn.pin; }
uint8_t controls_getConfirmPin(void) { return s_confirmBtn.pin; }

// Read raw digital state of a pin (convenience wrapper)
int controls_readPin(uint8_t pin) { return digitalRead(pin); }

float controls_getConfirmHoldProgress(void) {
  if (s_confirmBtn.stable == s_confirmBtn.idleState || s_confirmBtn.pressStart == 0 || s_confirmBtn.longFired) {
    return 0.0f;
  }
  unsigned long held = millis() - s_confirmBtn.pressStart;
  float p = (float)held / (float)s_longPressMs;
  if (p > 1.0f) p = 1.0f;
  if (p < 0.0f) p = 0.0f;
  return p;
}

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
            if (&b == &s_prevBtn) defaultPrevAction();
            else if (&b == &s_nextBtn) defaultNextAction();
            else if (&b == &s_confirmBtn) defaultConfirmAction();
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
  _pollButton(s_prevBtn, s_prev_cb, (s_prev_long_cb ? s_prev_long_cb : s_longpress_cb));
  _pollButton(s_nextBtn, s_next_cb, (s_next_long_cb ? s_next_long_cb : s_longpress_cb));
  _pollButton(s_confirmBtn, s_confirm_cb, (s_confirm_long_cb ? s_confirm_long_cb : s_longpress_cb));
}
