#pragma once

/*
 * epaper_monitor/display.h
 *
 * Public API for controlling the e-paper display used by the example.
 *
 * Responsibilities:
 *  - Initialize the display hardware
 *  - Provide simple helpers to render text and images (bitplane data)
 *  - Expose a small set of query functions (size / partial update support)
 *
 * Notes:
 *  - Image bitplanes are expected in row-major order, MSB-first per byte.
 *  - Supported image formats:
 *      - "bw" : single plane (black/white). bytes = ceil(width*height/8)
 *      - "3c" : two planes concatenated: [black_plane][red_plane]
 *               (each plane width*height/8 bytes)
 */

#include <Arduino.h>
#include <vector>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the display hardware. Must be called before other epd_* calls.
void epd_init();

// Draw a centered single-line text string on the display.
// - `txt` : the text to display (if empty, no action)
// - `color` : use the GxEPD_* color constants (e.g. GxEPD_RED or GxEPD_BLACK)
// - `forceFull` : when true, perform a full update even if partial updates are supported
void epd_displayText(const String &txt, uint16_t color, bool forceFull = false);

// Perform a recovery-style full clear (white/black cycles) to remove artifacts.
void epd_forceClear(void);
// Non-blocking request: schedule a force-clear on a background task if the
// display is not currently busy. Returns true if the job was scheduled.
bool epd_forceClear_async(void);

// Simple full white clear without the recovery black/white cycles
void epd_clear(void);

// Query helpers:
bool     epd_hasPartialUpdate(void); // whether the panel supports partial updates
uint16_t epd_width(void);
uint16_t epd_height(void);
 
// Runtime partial update control (can be toggled at runtime)
// - epd_setPartialEnabled toggles whether the driver will attempt partial updates
// - epd_getPartialEnabled returns the current runtime setting
void epd_setPartialEnabled(bool enabled);
bool epd_getPartialEnabled(void);

// Returns true if a long-running EPD job is in progress (force-clear, full update)
bool epd_isBusy(void);

// Run pending background EPD jobs. Must be called frequently (e.g., from loop()).
void epd_runBackgroundJobs(void);

// Draw an image from packed bitplane data.
// - `width`, `height` : image dimensions in pixels (must fit within display)
// - `data` : packed bytes (see comment above for per-format layout)
// - `format` : "bw" or "3c" (default "3c")
// - `color` : for "bw" format, choose "red" or "black" (default "red")
// - `forceFull` : force a full update (recommended for artifact-prone panels)
// Returns true on success, false on invalid inputs (wrong size/format).
bool epd_drawImageFromBitplanes(int width,
                                int height,
                                const std::vector<uint8_t> &data,
                                const char *format = "3c",
                                const char *color = "red",
                                bool forceFull = false);

// Retrieve the last set text (for status endpoints)
String epd_getCurrentText(void);

#ifdef __cplusplus
} // extern \"C\"
#endif
