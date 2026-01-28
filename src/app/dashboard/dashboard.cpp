#include "dashboard.h"
#include "app/wifi/wifi.h"
#include "drivers/epaper/display.h"
#include "app/controls/controls.h"
#include "app/ui/ui.h"
#include "utils/logger/logger.h"
#include "utils/base64.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <GxEPD2_3C.h>

// --- Static Helpers ---

static void send_error(WebServer* server, int code, const char* msg) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
    server->send(code, "application/json", buf);
}

static void send_success(WebServer* server, const char* action = NULL) {
    if (action) {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"action\":\"%s\"}", action);
        server->send(200, "application/json", buf);
    } else {
        server->send(200, "application/json", "{\"status\":\"ok\"}");
    }
}

static void serve_file_from_littlefs(WebServer* server, const char* path, const char* mime) {
  if (!LittleFS.exists(path)) {
    server->send(404, "text/plain", "Not found");
    return;
  }
  File f = LittleFS.open(path, "r");
  if (!f) {
    server->send(500, "text/plain", "Failed to open file");
    return;
  }
  server->streamFile(f, mime);
  f.close();
}

// --- Request Handlers ---

static WebServer* g_server = nullptr;

static void handleRoot() {
  if(g_server) serve_file_from_littlefs(g_server, "/index.html", "text/html");
}

static void handleStatus() {
  if(!g_server) return;
  StaticJsonDocument<256> doc;
  IPAddress ip = wifi_getIP();
  doc["ip"] = ip.toString();
  doc["text"] = epd_getCurrentText();
  doc["partialSupported"] = epd_hasPartialUpdate();
  doc["partialEnabled"] = epd_getPartialEnabled();
  doc["epdBusy"] = epd_isBusy();
  String out;
  serializeJson(doc, out);
  g_server->send(200, "application/json", out);
}

static void handleLogs() {
  if(!g_server) return;
  const std::deque<String>& logs = logger_getLogs();
  size_t capacity = JSON_ARRAY_SIZE(logs.size()) + (logs.size() * 100); 
  DynamicJsonDocument doc(capacity);
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& line : logs) {
    arr.add(line);
  }
  String out;
  serializeJson(doc, out);
  g_server->send(200, "application/json", out);
}

static void handleSetText() {
  if(!g_server) return;
  String body = g_server->arg("plain");
  if (body.length() == 0 && g_server->hasArg("text")) {
    String text = g_server->arg("text");
    String color = g_server->arg("color");
    uint16_t col = (color == "black") ? GxEPD_BLACK : GxEPD_RED;
    logger_log("SetText (form): %s", text.c_str());
    epd_displayText(text, col, false);
    send_success(g_server);
    return;
  }

  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    logger_log("SetText error: invalid json");
    send_error(g_server, 400, "invalid json");
    return;
  }
  const char* txt = doc["text"] | "";
  const char* color = doc["color"] | "red";
  bool forceFull = doc["forceFull"] | false;

  uint16_t col = GxEPD_RED;
  if (strcmp(color, "black") == 0) col = GxEPD_BLACK;

  logger_log("SetText: %s (%s)", txt, color);
  epd_displayText(String(txt), col, forceFull);

  StaticJsonDocument<128> res;
  res["status"] = "ok";
  res["text"] = txt;
  String out; serializeJson(res, out);
  g_server->send(200, "application/json", out);
}

static void handleImageUpload() {
  if(!g_server) return;
  String body = g_server->arg("plain");
  if (body.length() == 0) {
    send_error(g_server, 400, "empty body");
    return;
  }
  DynamicJsonDocument doc(body.length() + 1024);
  auto err = deserializeJson(doc, body);
  if (err) {
    send_error(g_server, 400, "invalid json");
    return;
  }
  int width = doc["width"] | 0;
  int height = doc["height"] | 0;
  const char* data_b64 = doc["data"] | "";
  const char* format = doc["format"] | "3c";
  const char* color = doc["color"] | "red";
  bool forceFull = doc["forceFull"] | false;

  if (width <= 0 || height <= 0 || strlen(data_b64) == 0) {
    send_error(g_server, 400, "missing fields");
    return;
  }

  std::vector<uint8_t> img;
  if (!base64_decode(String(data_b64), img)) {
    logger_log("ImageUpload: base64 error");
    send_error(g_server, 400, "base64 decode failed");
    return;
  }

  logger_log("ImageUpload: %dx%d %s", width, height, format);
  bool ok = epd_drawImageFromBitplanes(width, height, img, format, color, forceFull);
  if (!ok) {
    logger_log("ImageUpload: draw failed");
    send_error(g_server, 400, "invalid image or format");
    return;
  }

  StaticJsonDocument<128> res;
  res["status"] = "ok";
  res["width"] = width;
  res["height"] = height;
  res["format"] = format;
  String out; serializeJson(res, out);
  g_server->send(200, "application/json", out);
}

static void handleClear() {
  logger_log("Cmd: Clear");
  epd_clear();
  if(g_server) send_success(g_server, "cleared");
}

static void handleButtonNext() { ui_next(); if(g_server) send_success(g_server, "next"); }
static void handleButtonSelect() { ui_select(); if(g_server) send_success(g_server, "select"); }
static void handleButtonBack() { ui_back(); if(g_server) send_success(g_server, "back"); }

// --- App Interface Implementation ---

static void dashboard_registerRoutes(void* serverPtr) {
    g_server = (WebServer*)serverPtr;
    
    g_server->on("/", HTTP_GET, handleRoot);
    g_server->on("/app.js", HTTP_GET, [](){ serve_file_from_littlefs(g_server, "/app.js", "application/javascript"); });
    g_server->on("/style.css", HTTP_GET, [](){ serve_file_from_littlefs(g_server, "/style.css", "text/css"); });
    
    g_server->on("/status", HTTP_GET, handleStatus);
    g_server->on("/logs", HTTP_GET, handleLogs);
    g_server->on("/text", HTTP_POST, handleSetText);
    g_server->on("/image", HTTP_POST, handleImageUpload);
    g_server->on("/button/next", HTTP_POST, handleButtonNext);
    g_server->on("/button/select", HTTP_POST, handleButtonSelect);
    g_server->on("/button/back", HTTP_POST, handleButtonBack);
    
    // aliases
    g_server->on("/img", HTTP_POST, handleImageUpload);
    g_server->on("/clear", HTTP_POST, handleClear);
    g_server->on("/clear", HTTP_GET, handleClear);
    
    g_server->on("/diag", HTTP_GET, [](){
        StaticJsonDocument<128> doc;
        doc["prevPin"] = controls_getPrevPin();
        doc["nextPin"] = controls_getNextPin();
        doc["confirmPin"] = controls_getConfirmPin();
        doc["prevRaw"] = controls_readPin(controls_getPrevPin());
        doc["nextRaw"] = controls_readPin(controls_getNextPin());
        doc["confirmRaw"] = controls_readPin(controls_getConfirmPin());
        String out; serializeJson(doc, out);
        g_server->send(200, "application/json", out);
    });

    g_server->on("/ui_state", HTTP_GET, [](){
        StaticJsonDocument<64> doc;
        doc["state"] = ui_getState();
        doc["index"] = ui_getIndex();
        doc["epdBusy"] = epd_isBusy();
        doc["inApp"] = ui_isInApp();
        String out; serializeJson(doc, out);
        g_server->send(200, "application/json", out);
    });
}

// Render Preview for UI Carousel
static void dashboard_renderPreview(int16_t x, int16_t y) {
    // Just a placeholder or simple text
    // The previous implementation didn't strictly have a dashboard "app" in the carousel
    // but we can add one if we want "Home" to be selectable. 
    // For now we can assume this is just for the web server part. 
    // If it's not in the carousel, this function might not be called.
    // However, to satisfy App struct:
}

// On Select
static void dashboard_onSelect() {
    // No specific on-device UI for dashboard yet
}

const App APP_DASHBOARD = {
    .name = "Dashboard",
    .renderPreview = dashboard_renderPreview,
    .onSelect = dashboard_onSelect,
    .setup = nullptr,
    .registerRoutes = dashboard_registerRoutes,
    .poll = nullptr
};
