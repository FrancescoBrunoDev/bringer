#define ENABLE_GxEPD2_GFX 0

#include <Arduino.h>
#include <GxEPD2_3C.h>

#include "secrets.h"
#include "epaper_monitor/config.h"
#include "epaper_monitor/display/display.h"
#include "epaper_monitor/wifi.h"
#include "epaper_monitor/server/server.h"

/*
  Minimal main.ino after refactor
  - Initializes serial, display, WiFi and HTTP server
  - Keeps loop() minimal: delegate request handling to the server module
*/

// Buttons handled in controls module (src/epaper_monitor/controls)
// Defaults: clear=12, toggle=13
#include "epaper_monitor/controls/controls.h"
#include "epaper_monitor/ui/ui.h"

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize controls (buttons): pins are selectable at compile time via
  // -D CONTROL_CLEAR_PIN and -D CONTROL_TOGGLE_PIN build flags. Defaults chosen
  // here are conservative common GPIOs that typically are not strapping pins.
// Preferences: use GPIO11 and GPIO9 for buttons (11 = clear, 9 = toggle)
// Control pins (can be overridden with build flags)
#ifndef CONTROL_CLEAR_PIN
#define CONTROL_CLEAR_PIN 11
#endif
#ifndef CONTROL_TOGGLE_PIN
#define CONTROL_TOGGLE_PIN 9
#endif
  controls_init(CONTROL_CLEAR_PIN, CONTROL_TOGGLE_PIN);

  // Initialize display hardware (SPI init is done inside epd_init)
  epd_init();

  // Connect to WiFi (STA) or start AP fallback
  connectWiFi();

  // Start the HTTP server and register endpoints
  server_init();

  // Initialize UI (OLED menu and button callbacks)
  ui_init();

  // Show initial text (display module stores default text)
  epd_displayText(epd_getCurrentText(), GxEPD_RED, false);

  Serial.println("Setup complete");
}

void loop() {
  // Handle incoming HTTP requests
  server_handleClient();

  // Poll buttons via the controls module
  controls_poll();

  // Poll the UI (home clock / WiFi updates)
  ui_poll();

  // Run any pending background display jobs (non-blocking to callers)
  epd_runBackgroundJobs();

  // Nothing else here; display updates happen inside request handlers.
}
