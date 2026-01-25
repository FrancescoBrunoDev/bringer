/*
  server.cpp

  HTTP server implementation and request handlers for the ESP32 e-paper API example.

  - Registers endpoints:
      GET  /        -> simple HTML UI (served from LittleFS: /index.html, /app.js, /style.css)
      GET  /status  -> JSON status (ip, currentText, partialSupported)
      GET  /logs    -> JSON array of recent log messages
      POST /text    -> JSON { "text":"...", "color":"red"|"black", "forceFull":true|false }
      POST /image   -> JSON { "width":..., "height":..., "data":"<BASE64>", "format":"bw"|"3c", "color":"red"|"black", "forceFull":false }
      POST /img     -> alias for /image
      POST /clear   -> clears the display (full white)

  Note: The web UI is served only from LittleFS (files placed in the repository `data/` directory
  and uploaded to the device with `pio run -e esp32c6-devkitm-1 -t uploadfs`). There is no embedded
  HTML/CSS/JS fallback in the firmware — if the files are missing requests will return 404.
*/

#include "server.h"
#include "drivers/epaper/display.h"
#include "app/wifi/wifi.h"
#include "utils/base64.h"
#include "utils/logger/logger.h" // Centralized logger
#include "app/ui/ui.h"
#include "app/controls/controls.h"
#include "config.h"
#include "app/routes/text_app/text_app.h"

#include <WebServer.h>
#include <ArduinoJson.h>
#include <GxEPD2_3C.h> // for GxEPD_BLACK/GxEPD_RED
#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

// Local server instance
static WebServer server(WEB_SERVER_PORT);

// --- Handlers ---
static void serve_file_from_littlefs(const char* path, const char* mime) {
  // Serve a static file from LittleFS. If the file is missing, return 404.
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  File f = LittleFS.open(path, "r");
  if (!f) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }
  server.streamFile(f, mime);
  f.close();
}

static void handleRoot() {
  // Serve index.html from LittleFS (no embedded fallback).
  serve_file_from_littlefs("/index.html", "text/html");
}

static void handleStatus() {
  StaticJsonDocument<256> doc;
  IPAddress ip = wifi_getIP();
  doc["ip"] = ip.toString();
  doc["text"] = epd_getCurrentText();
  doc["partialSupported"] = epd_hasPartialUpdate();
  // Runtime setting: whether partial updates are currently enabled
  doc["partialEnabled"] = epd_getPartialEnabled();
  // EPD busy state
  doc["epdBusy"] = epd_isBusy();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleLogs() {
  // Return logs as a JSON array of strings
  const std::deque<String>& logs = logger_getLogs();
  
  // Calculate approximate size
  size_t capacity = JSON_ARRAY_SIZE(logs.size()) + (logs.size() * 100); 
  DynamicJsonDocument doc(capacity);
  JsonArray arr = doc.to<JsonArray>();
  
  for (const auto& line : logs) {
    arr.add(line);
  }
  
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleSetText() {
  String body = server.arg("plain");
  if (body.length() == 0 && server.hasArg("text")) {
    // form-encoded fallback
    String text = server.arg("text");
    String color = server.arg("color");
    uint16_t col = (color == "black") ? GxEPD_BLACK : GxEPD_RED;
    logger_log("SetText (form): %s", text.c_str());
    epd_displayText(text, col, false);
    server.send(200, "application/json", "{\"status\":\"ok\"}");
    return;
  }

  // Parse JSON body
  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    logger_log("SetText error: invalid json");
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  const char* txt = doc["text"] | "";
  const char* color = doc["color"] | "red";
  bool forceFull = doc["forceFull"] | false;

  uint16_t col = GxEPD_RED;
  if (strcmp(color, "black") == 0) col = GxEPD_BLACK;

  // Update display
  logger_log("SetText: %s (%s)", txt, color);
  epd_displayText(String(txt), col, forceFull);

  StaticJsonDocument<128> res;
  res["status"] = "ok";
  res["text"] = txt;
  String out; serializeJson(res, out);
  server.send(200, "application/json", out);
}

static void handleImageUpload() {
  String body = server.arg("plain");
  if (body.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"empty body\"}");
    return;
  }

  // Use a DynamicJsonDocument sized to the payload
  DynamicJsonDocument doc(body.length() + 1024);
  auto err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  int width = doc["width"] | 0;
  int height = doc["height"] | 0;
  const char* data_b64 = doc["data"] | "";
  const char* format = doc["format"] | "3c";
  const char* color = doc["color"] | "red";
  bool forceFull = doc["forceFull"] | false;

  if (width <= 0 || height <= 0 || strlen(data_b64) == 0) {
    server.send(400, "application/json", "{\"error\":\"missing fields\"}");
    return;
  }

  // Decode base64 payload
  std::vector<uint8_t> img;
  if (!base64_decode(String(data_b64), img)) {
    logger_log("ImageUpload: base64 error");
    server.send(400, "application/json", "{\"error\":\"base64 decode failed\"}");
    return;
  }

  // Let display module validate size and format
  logger_log("ImageUpload: %dx%d %s", width, height, format);
  bool ok = epd_drawImageFromBitplanes(width, height, img, format, color, forceFull);
  if (!ok) {
    logger_log("ImageUpload: draw failed");
    server.send(400, "application/json", "{\"error\":\"invalid image or format\"}");
    return;
  }

  StaticJsonDocument<128> res;
  res["status"] = "ok";
  res["width"] = width;
  res["height"] = height;
  res["format"] = format;
  String out; serializeJson(res, out);
  server.send(200, "application/json", out);
}

static void handleClear() {
  // Simple full white clear
  logger_log("Cmd: Clear");
  epd_clear();
  server.send(200, "application/json", "{\"status\":\"cleared\"}");
}

// Virtual button handlers (simulate physical buttons via HTTP)
static void handleButtonNext() {
  ui_next();
  server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"next\"}");
}

static void handleButtonSelect() {
  ui_select();
  server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"select\"}");
}

static void handleButtonBack() {
  ui_back();
  server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"back\"}");
}

// --- Public API ---
void server_init() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/app.js", HTTP_GET, [](){ serve_file_from_littlefs("/app.js", "application/javascript"); });
  server.on("/style.css", HTTP_GET, [](){ serve_file_from_littlefs("/style.css", "text/css"); });
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/text", HTTP_POST, handleSetText);
  server.on("/image", HTTP_POST, handleImageUpload);
  server.on("/button/next", HTTP_POST, handleButtonNext);
  server.on("/button/select", HTTP_POST, handleButtonSelect);
  server.on("/button/back", HTTP_POST, handleButtonBack);
  // backward-compatible alias
  server.on("/img", HTTP_POST, handleImageUpload);
  server.on("/clear", HTTP_POST, handleClear);
  // allow quick testing from browsers / curl (GET) as well
  server.on("/clear", HTTP_GET, handleClear);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  // Diagnostic endpoint: return current configured pins and raw readings
  server.on("/diag", HTTP_GET, [](){
    StaticJsonDocument<128> doc;
    doc["clearPin"] = controls_getClearPin();
    doc["togglePin"] = controls_getTogglePin();
    doc["clearRaw"] = controls_readPin(controls_getClearPin());
    doc["toggleRaw"] = controls_readPin(controls_getTogglePin());
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // Simple UI reflection endpoint: returns current UI state/index so web UI can mirror it
  server.on("/ui_state", HTTP_GET, [](){
    StaticJsonDocument<64> doc;
    doc["state"] = ui_getState();
    doc["index"] = ui_getIndex();
    doc["epdBusy"] = epd_isBusy();
    doc["inApp"] = ui_isInApp();
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // Register routes provided by apps (e.g. /apps/text/*)
  text_app_register(&server);

  server.begin();
  logger_log("HTTP server started");
}

void server_handleClient() {
  server.handleClient();
}


