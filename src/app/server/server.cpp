/*
  server.cpp

  HTTP server implementation and request handlers for the ESP32 e-paper API example.

  - Registers endpoints:
      GET  /        -> simple HTML UI
      GET  /status  -> JSON status (ip, currentText, partialSupported)
      POST /text    -> JSON { "text":"...", "color":"red"|"black", "forceFull":true|false }
      POST /image   -> JSON { "width":..., "height":..., "data":"<BASE64>", "format":"bw"|"3c", "color":"red"|"black", "forceFull":false }
      POST /img     -> alias for /image
      POST /clear   -> clears the display (full white)
*/

#include "server.h"
#include "drivers/epaper/display.h"
#include "app/wifi/wifi.h"
#include "utils/base64.h"
#include "app/ui/ui.h"
#include "app/controls/controls.h"
#include "config.h"
#include "app/routes/text_app/text_app.h"

#include <WebServer.h>
#include <ArduinoJson.h>
#include <GxEPD2_3C.h> // for GxEPD_BLACK/GxEPD_RED
#include <Arduino.h>
#include <vector>

// Local server instance
static WebServer server(WEB_SERVER_PORT);

// --- Handlers ---
static void handleRoot() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><title>ESP32 Menu</title></head><body>";
  html += "<h2>Device Menu</h2>";

  html += "<div id='menu'><ul style='list-style:none;padding-left:0;'>";
  html += "<li id='m0'>Home</li>";
  html += "<li id='m1'>Text App</li>";
  html += "<li id='m2'>Settings</li>";
  html += "</ul></div>";

  html += "<div id='settingsArea' style='display:none;'>";
  html += "<h3>Settings</h3>";
  html += "<p id='ipline'></p>";
  html += "<p><button onclick='postSetting(0)'>IP (noop)</button> <button onclick='postSetting(1)'>Toggle Partial</button> <button onclick='postSetting(2)'>Full Clean</button></p>";
  html += "</div>";
  html += "<div id='appArea' style='display:none;margin-top:12px;'><h3>Text App</h3><div id='textOptions'></div><p><button onclick='enterApp()'>Enter app</button> <button onclick='exitApp()'>Exit app</button></p></div>";
  html += "<p><button onclick='btnNext()'>Next</button> <button onclick='btnSelect()'>Select</button> <button onclick='btnBack()'>Back</button></p>";
  html += "<p><button onclick='doDiag()'>Run diag</button> <span id='diag'></span></p>";

  // EPD busy indicator (updated by client-side polling)
  html += "<div id='epdBusy' style='display:none;color:#b00;font-weight:bold;margin-top:8px;'>EPD updating...</div>";

  html += "<script>\n";
  html += "var lastState = -1;\n";
  html += "async function refresh(){\n";
  html += "  try{\n";
  html += "    let r=await fetch('/ui_state');\n";
  html += "    let j=await r.json();\n";
  html += "    let s=j.state;\n";
  html += "    let busy = j.epdBusy;\n";
  html += "    document.getElementById('m0').style.fontWeight='normal';\n";
  html += "    document.getElementById('m1').style.fontWeight='normal';\n";
  html += "    document.getElementById('m2').style.fontWeight='normal';\n";
  html += "    if (s==0) document.getElementById('m0').style.fontWeight='bold';\n";
  html += "    else if (s==1) document.getElementById('m1').style.fontWeight='bold';\n";
  html += "    else if (s==2) document.getElementById('m2').style.fontWeight='bold';\n";
  html += "    if (s==3){ document.getElementById('settingsArea').style.display='block'; refreshSettings(); } else { document.getElementById('settingsArea').style.display='none'; }\n";
  html += "    document.getElementById('epdBusy').style.display = busy ? 'block' : 'none';\n";
  html += "    // App area handling: show/hide and refresh list when entering state 1\n";
  html += "    if (s==1) {\n";
  html += "      document.getElementById('appArea').style.display = 'block';\n";
  html += "      if (lastState !== 1) loadTextOptions();\n";
  html += "    } else {\n";
  html += "      document.getElementById('appArea').style.display = 'none';\n";
  html += "    }\n";
  html += "    lastState = s;\n";
  html += "  } catch(e){ console.log(e); }\n";
  html += "}\n";
  html += "async function loadTextOptions(){\n";
  html += "  try{\n";
  html += "    let r = await fetch('/apps/text/list');\n";
  html += "    let j = await r.json();\n";
  html += "    let html = '';\n";
  html += "    for(let i=0;i<j.options.length;i++){\n";
  html += "      html += '<div style=\"margin-bottom:6px\">' + i + ': ' + j.options[i] + ' <button onclick=\"postSelect('+i+')\">Show</button></div>';\n";
  html += "    }\n";
  html += "    document.getElementById('textOptions').innerHTML = html;\n";
  html += "  } catch(e){ console.log(e); }\n";
  html += "}\n";
  html += "function postSelect(i){ fetch('/apps/text/select', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({index:i})}).then(refresh); }\n";
  html += "function enterApp(){ fetch('/button/select',{method:'POST'}).then(refresh); }\n";
  html += "function exitApp(){ fetch('/button/back',{method:'POST'}).then(refresh); }\n";
  html += "async function refreshSettings(){ try{ let r=await fetch('/status'); let j=await r.json(); document.getElementById('ipline').innerText = 'IP: ' + j.ip + ' | Partial: ' + (j.partialEnabled ? 'ON' : 'OFF'); }catch(e){console.log(e);} }\n";
  html += "function btnNext(){ fetch('/button/next',{method:'POST'}).then(refresh); }\n";
  html += "function btnSelect(){ fetch('/button/select',{method:'POST'}).then(refresh); }\n";
  html += "function btnBack(){ fetch('/button/back',{method:'POST'}).then(refresh); }\n";
  html += "function postSetting(i){ fetch('/button/next',{method:'POST'}).then(()=>fetch('/button/select',{method:'POST'})).then(refresh); }\n";
  html += "function doDiag(){ fetch('/diag').then(r=>r.json()).then(j=>document.getElementById('diag').innerText=JSON.stringify(j)); }\n";
  html += "setInterval(refresh,700); refresh();\n";
  html += "</script>";

  html += "</body></html>";
  server.send(200, "text/html", html);
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

static void handleSetText() {
  String body = server.arg("plain");
  if (body.length() == 0 && server.hasArg("text")) {
    // form-encoded fallback
    String text = server.arg("text");
    String color = server.arg("color");
    uint16_t col = (color == "black") ? GxEPD_BLACK : GxEPD_RED;
    epd_displayText(text, col, false);
    server.send(200, "application/json", "{\"status\":\"ok\"}");
    return;
  }

  // Parse JSON body
  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  const char* txt = doc["text"] | "";
  const char* color = doc["color"] | "red";
  bool forceFull = doc["forceFull"] | false;

  uint16_t col = GxEPD_RED;
  if (strcmp(color, "black") == 0) col = GxEPD_BLACK;

  // Update display
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
    server.send(400, "application/json", "{\"error\":\"base64 decode failed\"}");
    return;
  }

  // Let display module validate size and format
  bool ok = epd_drawImageFromBitplanes(width, height, img, format, color, forceFull);
  if (!ok) {
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
  server.on("/status", HTTP_GET, handleStatus);
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
  Serial.println("HTTP server started");
}

void server_handleClient() {
  server.handleClient();
}
