#define ENABLE_GxEPD2_GFX 0

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <LittleFS.h>

#include "secrets.h"
#include "config.h"
#include "drivers/epaper/display.h"
#include "app/wifi/wifi.h"
#include "app/server/server.h"
#include "app/beszel/beszel.h"

/*
  Minimal main.ino after refactor
  - Initializes serial, display, WiFi and HTTP server
  - Keeps loop() minimal: delegate request handling to the server module
*/

// Buttons handled in controls module (src/app/controls)
// Defaults: clear=12, toggle=13
#include "app/controls/controls.h"
#include "app/ui/ui.h"

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize controls (buttons): pins are selectable at compile time via
  // -D CONTROL_PREV_PIN, -D CONTROL_NEXT_PIN and -D CONTROL_CONFIRM_PIN build flags.
  // Defaults chosen here are conservative common GPIOs that typically are not strapping pins.
  // Preferences: use GPIO11 (Prev), GPIO9 (Next) and GPIO10 (Confirm) for buttons
  // Control pins (can be overridden with build flags)
#ifndef CONTROL_PREV_PIN
#define CONTROL_PREV_PIN 11
#endif
#ifndef CONTROL_NEXT_PIN
#define CONTROL_NEXT_PIN 9
#endif
#ifndef CONTROL_CONFIRM_PIN
#define CONTROL_CONFIRM_PIN 10
#endif
  controls_init(CONTROL_PREV_PIN, CONTROL_NEXT_PIN, CONTROL_CONFIRM_PIN);

  // Initialize display hardware (SPI init is done inside epd_init)
  epd_init();

  // Connect to WiFi (STA) or start AP fallback
  connectWiFi();

  // Mount LittleFS so the device can serve static files (index.html / app.js / style.css).
  // If the mount fails the web UI files will not be available and UI requests will return 404.
  if (LittleFS.begin()) {
    Serial.println("LittleFS mounted");
  } else {
    Serial.println("LittleFS mount failed: web UI files not available; UI requests will return 404");
  }

  // Start the HTTP server and register endpoints
  server_init();

  // Initialize UI (OLED menu and button callbacks)
  ui_init();

  // Initialize Beszel Service
  BeszelService::getInstance().begin("https://beszel.francesco-bruno.com/");

  // Show initial text (display module stores default text)
  // epd_displayText(epd_getCurrentText(), GxEPD_RED, false);

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
