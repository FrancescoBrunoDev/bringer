#define ENABLE_GxEPD2_GFX 0

#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <LittleFS.h>

#include "secrets.h"
#include "config.h"
#include "drivers/epaper/display.h"
#include "app/wifi/wifi.h"
#include "app/server/server.h"

// Apps
#include "app/registry.h"
#include "app/dashboard/dashboard.h"
#include "app/epub/epub.h"
// Beszel App (wrapper)
extern const App APP_BESZEL; // Defined in src/app/beszel/app.cpp
// RSS App
extern const App APP_RSS;    // Defined in src/app/rss/app.cpp
// Settings App
extern const App APP_SETTINGS; // Defined in src/app/settings/app.cpp

#include "app/controls/controls.h"
#include "app/ui/ui.h"

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize controls (buttons): pins are defined in src/config.h
  controls_init(PIN_BUTTON_PREV, PIN_BUTTON_NEXT, PIN_BUTTON_CONFIRM);

  // Initialize display hardware
  epd_init();

  // Connect to WiFi
  connectWiFi();

  // Mount LittleFS
  if (LittleFS.begin()) {
    Serial.println("LittleFS mounted");
  } else {
    Serial.println("LittleFS mount failed");
  }

  // Register Apps
  AppRegistry::registerApp(&APP_DASHBOARD); // System App (0)
  AppRegistry::registerApp(&APP_EPUB);      // Epub Reader (1)
  AppRegistry::registerApp(&APP_RSS);       // RSS Reader (2)
  AppRegistry::registerApp(&APP_BESZEL);    // Beszel Client (3)
  AppRegistry::registerApp(&APP_SETTINGS);  // Settings (4)

  // Run App Setups (e.g. Beszel init)
  AppRegistry::setupAll();

  // Start HTTP server (will register routes from all apps)
  server_init();

  // Initialize UI
  ui_init();

  Serial.println("Setup complete");
}

void loop() {
  // Handle HTTP requests
  server_handleClient();

  // Poll buttons
  controls_poll();

  // Poll UI
  ui_poll();
  
  // Poll Apps (background tasks)
  AppRegistry::pollAll();

  // Run display jobs
  epd_runBackgroundJobs();
}
