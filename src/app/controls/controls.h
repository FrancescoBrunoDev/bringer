#pragma once

/*
 * controls.h
 *
 * Modular button handling (Clear, Toggle Partial Update).
 *
 * - Software debounce
 * - Event callbacks (short press)
 * - Default behavior: Clear -> epd_clear(), Toggle -> epd_setPartialEnabled(!...)
 *
 * Note: Buttons are handled active-low (connect the other side of the button to GND
 * and configure the GPIO with INPUT_PULLUP).
 */

#include <Arduino.h>

using controls_button_cb_t = void (*)(void);

// Initialize the button handler.
// - prevPin: GPIO for the 'Prev' button (default 14)
// - nextPin: GPIO for the 'Next' button (default 16)
// - confirmPin: GPIO for the 'Confirm' button (default 9)
// - debounceMs: debounce time in ms (default 50)
// Note: This call is compatible with the older 2-argument signature; if the
// third parameter is not provided, `confirmPin` will use the default value.
void controls_init(uint8_t prevPin = 14, uint8_t nextPin = 16, uint8_t confirmPin = 9, unsigned long debounceMs = 50);

// Must be called frequently from the main loop to handle button polling
void controls_poll(void);

// Set callbacks invoked on short button presses (Prev / Next / Confirm)
void controls_setPrevCallback(controls_button_cb_t cb);
void controls_setNextCallback(controls_button_cb_t cb);
void controls_setConfirmCallback(controls_button_cb_t cb);

// (Optional) Callback for long-press events (global fallback)
// If individual long-press callbacks for buttons are not set, this will be used
void controls_setLongPressCallback(controls_button_cb_t cb);

// (Optional) Set long-press callback for individual buttons
void controls_setPrevLongCallback(controls_button_cb_t cb);
void controls_setNextLongCallback(controls_button_cb_t cb);
void controls_setConfirmLongCallback(controls_button_cb_t cb);

// (Compat) Keep historic names for compatibility; these are simple wrappers
void controls_setClearCallback(controls_button_cb_t cb);
void controls_setToggleCallback(controls_button_cb_t cb);
void controls_setClearLongCallback(controls_button_cb_t cb);
void controls_setToggleLongCallback(controls_button_cb_t cb);

// (Optional) Set long-press threshold in ms (default 1000 ms)
void controls_setLongPressMs(unsigned long ms);

// If true, the module registers default actions.
// If false, only registered callbacks will be invoked.
void controls_setUseDefaultActions(bool enable);

// Diagnostic helpers
// Return configured pins
uint8_t controls_getPrevPin(void);
uint8_t controls_getNextPin(void);
uint8_t controls_getConfirmPin(void);
// Return legacy names
uint8_t controls_getClearPin(void);
uint8_t controls_getTogglePin(void);
// Returns the hold progress (0.0 to 1.0) of the confirm button relative to the long-press threshold
float controls_getConfirmHoldProgress(void);

// Read raw digital state of a pin (convenience wrapper)
int controls_readPin(uint8_t pin);
