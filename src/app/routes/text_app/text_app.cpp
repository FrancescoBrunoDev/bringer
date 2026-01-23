/*
 * text_app.cpp
 *
 * Implements a minimal \"Text App\" that exposes a small set of selectable
 * text strings which can be rendered on the e-paper display. The module
 * registers HTTP endpoints under `/apps/text/*` and provides simple
 * introspection helpers for the OLED menu UI.
 *
 * Endpoints:
 *  - GET  /apps/text/list    -> { "options": [ "...", ... ] }
 *  - GET  /apps/text/count   -> { "count": N }
 *  - GET  /apps/text/current -> { "text": "...", "index": N } // index -1 if not in list
 *  - POST /apps/text/select  -> JSON { "index": N } or { "text": "..." , "color":"red"|"black" }
 *
 * Notes:
 *  - `text_app_register(void* webserver_ptr)` expects a pointer to the
 *    WebServer instance (pass `&server` from `server_init()`).
 */

#include "app/routes/text_app/text_app.h"
#include "drivers/epaper/display.h"
#include "drivers/oled/oled.h"

#include <WebServer.h>
#include <ArduinoJson.h>
#include <GxEPD2_3C.h>
#include <Arduino.h>

static const char* k_text_options[] = {
  "Hello API",
  "Ciao",
  "Buongiorno",
  "Benvenuto",
  "Testo di prova"
};
static constexpr size_t k_text_options_count = sizeof(k_text_options) / sizeof(k_text_options[0]);

// Find index of `txt` in k_text_options or -1 if not found
static int _find_option_index(const String &txt) {
  for (size_t i = 0; i < k_text_options_count; ++i) {
    if (txt == String(k_text_options[i])) return (int)i;
  }
  return -1;
}

#ifdef __cplusplus
extern "C" {
#endif

void text_app_register(void *webserver_ptr) {
  WebServer *srv = static_cast<WebServer*>(webserver_ptr);
  if (!srv) {
    Serial.println("text_app: invalid webserver pointer");
    return;
  }

  // List options
  srv->on("/apps/text/list", HTTP_GET, [srv]() {
    StaticJsonDocument<256> doc;
    JsonArray arr = doc.createNestedArray("options");
    for (size_t i = 0; i < k_text_options_count; ++i) {
      arr.add(k_text_options[i]);
    }
    String out;
    serializeJson(doc, out);
    srv->send(200, "application/json", out);
  });

  // Count
  srv->on("/apps/text/count", HTTP_GET, [srv]() {
    StaticJsonDocument<64> doc;
    doc["count"] = (int)k_text_options_count;
    String out; serializeJson(doc, out);
    srv->send(200, "application/json", out);
  });

  // Current displayed text + optional index if it matches one of the options
  srv->on("/apps/text/current", HTTP_GET, [srv]() {
    String cur = epd_getCurrentText();
    int idx = _find_option_index(cur);
    StaticJsonDocument<256> doc;
    doc["text"] = cur;
    doc["index"] = idx;
    String out; serializeJson(doc, out);
    srv->send(200, "application/json", out);
  });

  // Select an option (by index) or provide free text
  srv->on("/apps/text/select", HTTP_POST, [srv]() {
    // Read body (JSON preferred)
    String body = srv->arg("plain");
    String chosen_text;
    int chosen_index = -1;
    const char *color_cstr = "red";

    if (body.length() == 0) {
      // Fallback to form-encoded fields: index or text
      if (srv->hasArg("index")) {
        int idx = srv->arg("index").toInt();
        if (idx < 0 || (size_t)idx >= k_text_options_count) {
          srv->send(400, "application/json", "{\"error\":\"index out of range\"}");
          return;
        }
        chosen_index = idx;
        chosen_text = String(k_text_options[idx]);
      } else if (srv->hasArg("text")) {
        chosen_text = srv->arg("text");
      } else {
        srv->send(400, "application/json", "{\"error\":\"missing fields\"}");
        return;
      }
      if (srv->hasArg("color")) color_cstr = srv->arg("color").c_str();
    } else {
      // Parse JSON
      DynamicJsonDocument doc(body.length() + 256);
      auto err = deserializeJson(doc, body);
      if (err) {
        srv->send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
      }
      if (doc.containsKey("index")) {
        int idx = doc["index"] | -1;
        if (idx < 0 || (size_t)idx >= k_text_options_count) {
          srv->send(400, "application/json", "{\"error\":\"index out of range\"}");
          return;
        }
        chosen_index = idx;
        chosen_text = String(k_text_options[idx]);
      } else if (doc.containsKey("text")) {
        const char *t = doc["text"] | "";
        chosen_text = String(t);
      } else {
        srv->send(400, "application/json", "{\"error\":\"missing fields\"}");
        return;
      }
      const char *c = doc["color"] | "red";
      color_cstr = c;
    }

    if (chosen_text.length() == 0) {
      srv->send(400, "application/json", "{\"error\":\"empty text\"}");
      return;
    }

    // Convert color to GxEPD_* constant
    uint16_t color = GxEPD_RED;
    if (strcmp(color_cstr, "black") == 0) color = GxEPD_BLACK;

    // Prevent updating while a long-running job is in progress
    if (epd_isBusy()) {
      srv->send(503, "application/json", "{\"error\":\"epd busy\"}");
      return;
    }

    // Provide user feedback on OLED (menu mode may suppress larger statuses)
    if (oled_isAvailable()) {
      oled_showToast("Rendering...", 1200);
    }

    // Perform the display update (blocking)
    epd_displayText(chosen_text, color, false);

    // Build response
    StaticJsonDocument<192> res;
    res["status"] = "ok";
    res["text"] = chosen_text;
    res["index"] = chosen_index;
    res["color"] = color_cstr;
    String out; serializeJson(res, out);
    srv->send(200, "application/json", out);
  });

  Serial.println("text_app: routes registered (/apps/text/*)");
}

size_t text_app_get_count(void) {
  return k_text_options_count;
}

const char* text_app_get_text(size_t index) {
  if (index >= k_text_options_count) return nullptr;
  return k_text_options[index];
}

#ifdef __cplusplus
} // extern "C"
#endif