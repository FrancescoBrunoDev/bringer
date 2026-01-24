#pragma once

/*
 * ui.h
 *
 * Simple menu UI using the SSD1306 OLED for navigation:
 *
 * - Prev (short): scroll to previous menu item
 * - Next (short): scroll to next menu item
 * - Confirm: short press = select/confirm, long press = cancel/go back
 *
 * The initial menu shows 'Settings' with two entries:
 *  - Partial update ON/OFF
 *  - Full cleaning (runs a recovery-style full clear)
 *
 * The implementation registers button callbacks (via the `controls` module)
 * and updates the OLED using functions in `drivers/oled`.
 */

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the UI: register button callbacks and show the initial menu.
void ui_init(void);

// UI poll: call frequently (e.g. from loop) to update clock and WiFi status
void ui_poll(void);

// Scroll to next menu item (short Next press)
void ui_next(void);

// Scroll to previous menu item (short Prev press)
void ui_prev(void);

// Select the currently highlighted item / Confirm (short Confirm press)
void ui_select(void);

// Go back / Cancel (Confirm long press)
void ui_back(void);

// Introspection helpers for the web UI
int ui_getState(void);
int ui_getIndex(void);
// Returns true if the UI is currently in the Text App screen
bool ui_isInApp(void);

#ifdef __cplusplus
} // extern "C"
#endif
