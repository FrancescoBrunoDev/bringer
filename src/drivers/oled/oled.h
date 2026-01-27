#pragma once

/*
 * oled.h
 *
 * Helper for a small SSD1306 OLED (e.g. 128x64) over I2C.
 *
 * - Initializes the I2C bus and the display
 * - Provides simple APIs to show status messages (e.g. "Clearing", "Loading", "Done")
 * - Provides a fallback function to check whether the display is operational
 *
 * Typical usage:
 *   oled_init();                 // optional: pass SDA/SCL if not using defaults
 *   oled_showStatus(\"Loading\");
 *   oled_clear();
 *
 * Note: implementation uses I2C address 0x3C by default (many SSD1306 modules).
 */

#include <Arduino.h>
#include <stdint.h>



// Defaults (modifiable via oled_init parameters)
static constexpr uint8_t OLED_DEFAULT_SDA = 15;       // suggested: SDA -> GPIO15
static constexpr uint8_t OLED_DEFAULT_SCL = 22;       // suggested: SCL -> GPIO22
static constexpr uint8_t OLED_DEFAULT_I2C_ADDR = 0x3C;
static constexpr uint8_t OLED_WIDTH = 128;
static constexpr uint8_t OLED_HEIGHT = 64;

/**
 * oled_init
 * Initialize the I2C bus (Wire) and the SSD1306 display.
 * - sda, scl: GPIO pins for SDA/SCL (Wire.begin(sda, scl))
 * - address: I2C address (typically 0x3C)
 *
 * After a failed initialization, oled_isAvailable() will return false.
 */
void oled_init(uint8_t sda = OLED_DEFAULT_SDA,
               uint8_t scl = OLED_DEFAULT_SCL,
               uint8_t address = OLED_DEFAULT_I2C_ADDR);

/**
 * oled_isAvailable
 * Returns true if the OLED was successfully initialized and can be used.
 */
bool oled_isAvailable(void);

/**
 * oled_clear
 * Clear the screen (fill with white).
 */
void oled_clear(void);

/**
 * oled_clearBuffer
 * Clear only the internal buffer, without updating the screen.
 */
void oled_clearBuffer(void);

/**
 * oled_display
 * Send the current buffer to the display.
 */
void oled_display(void);

/**
 * oled_showStatus
 * Show a centered status message in large font (e.g. "Clearing", "Loading...").
 * An immediate display() is performed to update the screen.
 */
void oled_showStatus(const char *msg);
inline void oled_showStatus(const String &msg) { oled_showStatus(msg.c_str()); }

/**
 * oled_showLines
 * Show two lines in small font (useful to show IP and a sub-message).
 * Performs an immediate refresh.
 */
void oled_showLines(const char *line1, const char *line2, int16_t x_offset = 0, int16_t y_offset = 0, bool update = true);
inline void oled_showLines(const String &l1, const String &l2) { oled_showLines(l1.c_str(), l2.c_str(), 0, 0, true); }

/**
 * oled_showProgress
 * Show a message + numeric progress (e.g. "Clearing 1/4").
 */
void oled_showProgress(const char *msg, int current, int total);
inline void oled_showProgress(const String &msg, int current, int total) { oled_showProgress(msg.c_str(), current, total); }

/**
 * oled_setMenuMode
 * When enabled, the OLED is reserved for the menu UI and status/progress messages
 * from other modules (e.g. e-paper) will be suppressed.
 */
void oled_setMenuMode(bool enable);

/**
 * oled_isMenuMode
 * Returns true if the OLED is currently in "menu" mode.
 */
bool oled_isMenuMode(void);

/**
 * oled_showWifiIcon
 * Draw a simple WiFi icon (semicircles + dot) at the center of the display.
 * - connected: if true, draws the semicircles normally; if false, draws the same
 *   but the icon can be used to indicate state (caller decides text or serial fallback).
 */
void oled_showWifiIcon(bool connected);

/**
 * oled_drawHomeScreen
 * Draw the home screen with large centered time and small WiFi icon.
 * y_offset: vertical translation (0 is normal)
 * update: if true, calls display()
 */
void oled_drawHomeScreen(const char *time, bool wifiConnected, int16_t x_offset = 0, int16_t y_offset = 0, bool update = true);

/**
 * oled_drawAppPreview
 * Draw an app title large and centered (carousel mode).
 * y_offset: vertical translation (0 is normal)
 * update: if true, calls display()
 */
void oled_drawBigText(const char *text, int16_t x_offset = 0, int16_t y_offset = 0, bool update = true, bool hasHeader = false);

enum ToastPos { TOAST_TOP, TOAST_BOTTOM };
enum ToastIcon { TOAST_ICON_NONE, TOAST_ICON_UP, TOAST_ICON_DOWN, TOAST_ICON_SELECT, TOAST_ICON_BACK };

/**
 * oled_showToast
 * Show a transient, non-persistent overlay message.
 * - msg: text to show (can be NULL if only showing icon)
 * - ms: duration in milliseconds
 * - pos: vertical position (top/bottom)
 * - icon: optional arrow or select icon
 */
void oled_showToast(const char *msg, uint32_t ms, ToastPos pos = TOAST_BOTTOM, ToastIcon icon = TOAST_ICON_NONE);

/**
 * oled_drawToggle
 * Draw a modern graphical toggle switch with a label.
 */
void oled_drawToggle(const char *label, bool state, int16_t x_offset = 0, int16_t y_offset = 0);

/**
 * oled_drawHeader
 * Draw a persistent bar at the top with a title.
 */
void oled_drawHeader(const char *title, int16_t x_offset = 0, int16_t y_offset = 0);

/**
 * oled_drawScrollProgress
 * Draw a vertical progress bar on the left (2px wide).
 * - progress: 0.0 to 1.0
 */
void oled_drawScrollProgress(float progress);

/**
 * oled_showHoldToast
 * Show a toast that slides based on a manual progress value (0.0 to 1.0).
 * Useful for long-press feedback.
 * - pos: vertical position
 * - icon: icon to show
 * - progress: 0.0 (off-screen) to 1.0 (fully in)
 */
void oled_showHoldToast(ToastPos pos, ToastIcon icon, float progress);

/**
 * oled_drawActiveToast
 * Draw the current toast (if any) to the buffer.
 * Should be called by ui_redraw before closing the frame.
 */
void oled_drawActiveToast(void);

/**
 * oled_poll
 * Must be called frequently. Handles toast expiration and potential animations.
 * Returns true if the display needs a redraw because a toast changed state (e.g. vanished).
 */
bool oled_poll(void);
