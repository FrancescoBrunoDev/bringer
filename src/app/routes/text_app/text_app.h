#pragma once

/*
 * text_app.h
 *
 * Small \"Text App\" route: exposes a handful of text options that can be
 * displayed on the e-paper panel. This is intended to be a simple example
 * of an \"app\" that lives under `src/app/routes/...` and registers its
 * own HTTP endpoints during `server_init()`.
 *
 * Public API:
 *   - text_app_register(void *webserver_ptr)
 *       Register HTTP endpoints (accepts a pointer to the WebServer instance).
 *
 *   - text_app_get_count()
 *   - text_app_get_text(index)
 *       Introspection helpers used by the UI to enumerate selectable texts.
 *
 */

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Register HTTP endpoints for the Text App.
// The function accepts a pointer to the WebServer instance (opaque here to
// avoid including the WebServer header in this file). Call this from
// `server_init()` (pass `&server`).
void text_app_register(void *webserver_ptr);

// Returns how many text options are available.
size_t text_app_get_count(void);

// Returns the text at `index` (0-based). Returns nullptr if index out of range.
const char* text_app_get_text(size_t index);

#ifdef __cplusplus
} // extern \"C\"
#endif