// Minimal UI components implementation
#include "components.h"
#include "drivers/oled/oled.h"
#include "app/wifi/wifi.h"
#include "drivers/epaper/display.h"

#include <time.h>

void comp_title_and_text(const char *title, const char *text) {
  oled_showLines(title, text ? text : "");
}

void comp_time_and_wifi(void) {
  char timebuf[16];
  time_t now = time(nullptr);
  if (now > 1600000000) {
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(timebuf, sizeof(timebuf), "%H:%M", &tm);
  } else {
    unsigned long s = millis() / 1000;
    snprintf(timebuf, sizeof(timebuf), "%02lu:%02lu", (s/3600)%100, (s/60)%60);
  }
  oled_drawHomeScreen(timebuf, wifi_isConnected());
}

void comp_switch(const char *label, bool state) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s: %s", label, state ? "ON" : "OFF");
  oled_showLines(label, buf);
}

void comp_button(const char *label) {
  oled_showLines(label, "");
}
