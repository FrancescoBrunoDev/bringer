/*
  display.cpp

  Implementation of the e-paper display helper API used by the example.
  Responsibilities:
    - Initialize the display hardware
    - Draw centered text
    - Draw images from packed bitplanes (bw / 3c)
    - Provide small query helpers (size / partial update)
*/

#include "display.h"
#include "config.h"
#include "utils/base64.h" // only included for completeness; not used directly here
#include "drivers/oled/oled.h"

#include <SPI.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>

#include <algorithm>

static GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));
static U8G2_FOR_ADAFRUIT_GFX s_u8g2_epd;

// Internal state
static String g_currentText = "Hello API";
static uint16_t g_currentColor = GxEPD_RED;
static bool g_partialEnabled = ENABLE_PARTIAL_UPDATE;

void epd_init() {
  // Initialize SPI (explicit pins, some cores need explicit init)
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

  // Initialize OLED early so we can report progress/status to the user
  oled_init(); // defaults: SDA=15, SCL=16, addr=0x3C
  if (oled_isAvailable()) {
    oled_showStatus("EPD init...");
  }

  // Init e-paper display (keep it awake; we won't hibernate to serve future updates)
  display.init(115200, false, 50, false); // debug=false
  
  // Init U8g2 helper
  s_u8g2_epd.begin(display);
  s_u8g2_epd.setFont(u8g2_font_profont29_tr); // larger font for e-paper
  s_u8g2_epd.setForegroundColor(GxEPD_RED);
  s_u8g2_epd.setBackgroundColor(GxEPD_WHITE);

  /*
  if (ENABLE_FORCE_CLEAR) {
    if (oled_isAvailable()) {
      oled_showStatus("Recovery clearing...");
    }
    // Schedule a background force-clear so the system remains responsive
    if (!epd_forceClear_async()) {
      // If scheduling failed because the driver is busy, fall back to sync
      epd_forceClear();
    }
  }
  */

  if (oled_isAvailable()) {
    oled_showStatus("Ready");
  }

  // initial text draw will be performed by caller if desired
}

bool epd_hasPartialUpdate() {
  return display.epd2.hasPartialUpdate;
}

uint16_t epd_width() {
  return display.width();
}

uint16_t epd_height() {
  return display.height();
}

String epd_getCurrentText() {
  return g_currentText;
}

void epd_setPartialEnabled(bool enabled) {
  g_partialEnabled = enabled;
  if (oled_isAvailable()) {
    oled_showStatus(enabled ? "Partial ON" : "Partial OFF");
  }
}

bool epd_getPartialEnabled(void) {
  return g_partialEnabled;
}

void epd_forceClear() {
  Serial.println("Force clearing display (white/black cycles) to recover from noise/artifacts...");
  if (oled_isAvailable()) {
    oled_showStatus("Recovery clearing...");
  }
  // Run several full passes
  const int cycles = 4;
  for (int i = 0; i < cycles; ++i) {
    Serial.print("clear cycle ");
    Serial.println(i+1);
    if (oled_isAvailable()) {
      // show progress on the OLED: Clearing 1/4, 2/4...
      oled_showProgress("Clearing", i+1, cycles);
    }
    display.setFullWindow();
    display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
    delay(400);
    display.setFullWindow();
    display.firstPage();
    do { display.fillScreen(GxEPD_BLACK); } while (display.nextPage());
    delay(400);
  }
  // Final white pass
  display.setFullWindow();
  display.firstPage();
  do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
  delay(200);
  Serial.println("Clear done");
  if (oled_isAvailable()) {
    oled_showStatus("Cleared");
  }
}

void epd_clear() {
  if (oled_isAvailable()) {
    oled_showStatus("Clearing...");
  }
  // Simple full white clear (single pass)
  display.setFullWindow();
  display.firstPage();
  do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
  Serial.println("epd_clear: display cleared (full white pass)");
  if (oled_isAvailable()) {
    oled_showStatus("Cleared");
  }
}

void epd_displayText(const String &txt, uint16_t color, bool forceFull) {
  if (txt.length() == 0) return;

  // store state
  g_currentText = txt;
  g_currentColor = color;
  if (oled_isAvailable()) oled_showStatus("Rendering...");

  // Choose font and measure
  s_u8g2_epd.setFont(u8g2_font_profont29_tr);
  s_u8g2_epd.setBackgroundColor(GxEPD_WHITE);
  s_u8g2_epd.setForegroundColor(color);

  int16_t bw = s_u8g2_epd.getUTF8Width(txt.c_str());
  int16_t bh = s_u8g2_epd.getFontAscent() - s_u8g2_epd.getFontDescent();
  int16_t bx = 0; 
  int16_t by = 0; // U8g2 logic is different, we use width directly

  // If text too wide, fallback to smaller font
  if ((int)bw > (int)(display.width() - 8)) {
    s_u8g2_epd.setFont(u8g2_font_profont17_tr);
    bw = s_u8g2_epd.getUTF8Width(txt.c_str());
    bh = s_u8g2_epd.getFontAscent() - s_u8g2_epd.getFontDescent();
  }

  // Compute centered cursor position
  int16_t cx = ((display.width() - bw) / 2) - bx;
  int16_t cy = ((display.height() - bh) / 2) - by;

  // Compute update rectangle (small padding)
  const int pad = 4;
  int16_t rx = cx + bx - pad;
  int16_t ry = cy + by - pad;
  uint16_t rw = bw + pad * 2;
  uint16_t rh = bh + pad * 2;

  // Clamp region to display bounds
  if (rx < 0) { rw += rx; rx = 0; }
  if (ry < 0) { rh += ry; ry = 0; }
  if (rx + (int)rw > display.width()) rw = display.width() - rx;
  if (ry + (int)rh > display.height()) rh = display.height() - ry;

  // Attempt partial update if supported and not forced full
  bool usedPartial = false;
  unsigned long t0 = millis();
  if (g_partialEnabled && display.epd2.hasPartialUpdate && !forceFull) {
    usedPartial = true;
    display.setPartialWindow(rx, ry, rw, rh);
    display.firstPage();
    do {
      display.fillRect(rx, ry, rw, rh, GxEPD_WHITE);
      // Center calculation:
      int16_t x = (display.width() - bw) / 2;
      int16_t y = (display.height() / 2) + (s_u8g2_epd.getFontAscent()/2);
      
      s_u8g2_epd.setCursor(x, y);
      s_u8g2_epd.print(txt);
    } while (display.nextPage());
  } else {
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      // Center calculation:
      int16_t x = (display.width() - bw) / 2;
      int16_t y = (display.height() / 2) + (s_u8g2_epd.getFontAscent()/2);
      
      s_u8g2_epd.setCursor(x, y);
      s_u8g2_epd.print(txt);
    } while (display.nextPage());
  }
  unsigned long dt = millis() - t0;
  Serial.print("displayText: usedPartial=");
  Serial.print(usedPartial ? "yes":"no");
  Serial.print(" time(ms)=");
  Serial.println(dt);
  if (oled_isAvailable()) oled_showStatus("Done");
}

void epd_displayDate(time_t now) {
  struct tm tm;
  localtime_r(&now, &tm);
  char buf[64];
  // Format: DD/MM/YYYY
  strftime(buf, sizeof(buf), "%d/%m/%Y", &tm);
  // Use Red color for visibility and style
  epd_displayText(String(buf), GxEPD_RED);
}

static bool _draw_bw(int width, int height, const std::vector<uint8_t> &img, int rx, int ry, const char *color) {
  int bytesPerRow = (width + 7) / 8;
  uint16_t drawColor = GxEPD_RED;
  if (strcmp(color, "black") == 0) drawColor = GxEPD_BLACK;

  for (int y = 0; y < height; ++y) {
    int row = y * bytesPerRow;
    for (int x = 0; x < width; ++x) {
      int byteIndex = row + (x >> 3);
      uint8_t b = img[byteIndex];
      int bit = 7 - (x & 7);
      bool pixelOn = ((b >> bit) & 1) != 0;
      if (pixelOn) display.drawPixel(rx + x, ry + y, drawColor);
    }
  }
  return true;
}

static bool _draw_3c(int width, int height, const std::vector<uint8_t> &img, int rx, int ry) {
  int bytesPerRow = (width + 7) / 8;
  int bytesPerPlane = bytesPerRow * height;
  const uint8_t *blackPlane = img.data();
  const uint8_t *redPlane = img.data() + bytesPerPlane;

  for (int y = 0; y < height; ++y) {
    int row = y * bytesPerRow;
    for (int x = 0; x < width; ++x) {
      int byteIndex = row + (x >> 3);
      int bit = 7 - (x & 7);
      bool redOn = ((redPlane[byteIndex] >> bit) & 1) != 0;
      bool blackOn = ((blackPlane[byteIndex] >> bit) & 1) != 0;
      if (redOn) display.drawPixel(rx + x, ry + y, GxEPD_RED);
      else if (blackOn) display.drawPixel(rx + x, ry + y, GxEPD_BLACK);
    }
  }
  return true;
}

bool epd_drawImageFromBitplanes(int width,
                                int height,
                                const std::vector<uint8_t> &data,
                                const char *format,
                                const char *color,
                                bool forceFull) {
  if (width <= 0 || height <= 0) {
    if (oled_isAvailable()) oled_showStatus("Error");
    return false;
  }
  if (width > (int)display.width() || height > (int)display.height()) {
    if (oled_isAvailable()) oled_showStatus("Error: too large");
    return false;
  }

  int bytesPerRow = (width + 7) / 8;
  int bytesPerPlane = bytesPerRow * height;

  if (strcmp(format, "bw") == 0) {
    if ((int)data.size() < bytesPerPlane) {
      if (oled_isAvailable()) oled_showStatus("Error: size");
      return false;
    }
  } else if (strcmp(format, "3c") == 0) {
    if ((int)data.size() < bytesPerPlane * 2) {
      if (oled_isAvailable()) oled_showStatus("Error: size 3c");
      return false;
    }
  } else {
    if (oled_isAvailable()) oled_showStatus("Error: format");
    return false;
  }

  // Basic 3c overlap check (optional strictness)
  if (strcmp(format, "3c") == 0) {
    for (int i = 0; i < bytesPerPlane; ++i) {
      if ((data[i] & data[bytesPerPlane + i]) != 0) {
        // overlapping black/red bits - consider this invalid upload
        if (oled_isAvailable()) oled_showStatus("Error: overlap");
        return false;
      }
    }
  }

  // Indicate loading on OLED (if present)
  if (oled_isAvailable()) {
    oled_showStatus("Loading...");
  }

  // Center image on display
  int rx = ((int)display.width() - width) / 2;
  int ry = ((int)display.height() - height) / 2;

  unsigned long t0 = millis();
  bool usedPartial = false;
  if (g_partialEnabled && display.epd2.hasPartialUpdate && !forceFull) {
    usedPartial = true;
    display.setPartialWindow(rx, ry, width, height);
  } else {
    display.setFullWindow();
  }

  display.firstPage();
  do {
    if (usedPartial) {
      display.fillRect(rx, ry, width, height, GxEPD_WHITE);
    } else {
      display.fillScreen(GxEPD_WHITE);
    }

    if (strcmp(format, "bw") == 0) {
      _draw_bw(width, height, data, rx, ry, color);
    } else {
      _draw_3c(width, height, data, rx, ry);
    }
  } while (display.nextPage());

  unsigned long dt = millis() - t0;
  Serial.print("epd_drawImageFromBitplanes: format=");
  Serial.print(format);
  Serial.print(" usedPartial=");
  Serial.print(usedPartial ? "yes" : "no");
  Serial.print(" time(ms)=");
  Serial.println(dt);

  if (oled_isAvailable()) {
    oled_showStatus("Done");
  }

  return true;
}

// ---- Background async force-clear support ----
// A simple flag-driven background job: when epd_forceClear_async() is called
// we set a flag and the actual clear is performed in epd_runBackgroundJobs()
// which should be called often from loop(). This avoids blocking the main
// loop for long clears and allows controls/server to keep running.

static volatile bool s_forceClearRequested = false;
static volatile bool s_busy = false;

// Job state for incremental, non-blocking clear
enum ClearPhase { PHASE_WHITE = 0, PHASE_BLACK = 1, PHASE_FINAL_WHITE = 2 };
static const int s_totalCycles = 4;
static int s_job_cycle = 0;           // current cycle index (0..s_totalCycles-1)
static ClearPhase s_job_phase = PHASE_WHITE;
static bool s_job_active = false;     // whether a job is claimed
static bool s_job_inPage = false;     // whether we're mid-page sequence
static unsigned long s_job_lastStepMs = 0; // used for non-blocking delays between passes

bool epd_forceClear_async(void) {
  if (s_busy || s_job_active) return false;
  s_forceClearRequested = true;
  return true;
}

bool epd_isBusy(void) {
  return s_busy || s_job_active;
}

// Called frequently from loop(): runs at most a small amount of work per call
// to keep the main loop responsive. It performs the force-clear in an
// incremental, page-by-page manner with non-blocking delays between passes.
void epd_runBackgroundJobs(void) {
  // If there's a new request and no active job, claim it and initialize state
  if (!s_job_active && s_forceClearRequested) {
    s_forceClearRequested = false;
    s_job_active = true;
    s_busy = true;
    s_job_cycle = 0;
    s_job_phase = PHASE_WHITE;
    s_job_inPage = false;
    s_job_lastStepMs = millis();
    Serial.println("Background: Force clear requested (incremental)");
    if (oled_isAvailable()) oled_showStatus("Recovery clearing...");
  }

  if (!s_job_active) return;

  // If we are currently between passes, respect non-blocking delays
  unsigned long now = millis();
  unsigned long waitMs = 0;
  if (s_job_phase == PHASE_FINAL_WHITE) waitMs = 200;
  else waitMs = 400;
  if (!s_job_inPage && (now - s_job_lastStepMs) < waitMs) {
    // still waiting before starting next pass
    return;
  }

  // Determine color for current pass
  int drawColor = GxEPD_WHITE;
  if (s_job_phase == PHASE_BLACK) drawColor = GxEPD_BLACK;

  // Perform a single page of the current pass per invocation
  if (!s_job_inPage) {
    // start a new page sequence for this pass
    if (oled_isAvailable()) oled_showProgress("Clearing", s_job_cycle + 1, s_totalCycles);
    display.setFullWindow();
    display.firstPage();
    // draw the first page
    display.fillScreen(drawColor);
    bool more = display.nextPage();
    s_job_inPage = more;
    if (!more) {
      // finished this pass immediately
      s_job_inPage = false;
      s_job_lastStepMs = millis();
      // advance state
      if (s_job_phase == PHASE_WHITE) {
        s_job_phase = PHASE_BLACK;
      } else if (s_job_phase == PHASE_BLACK) {
        // completed a full white+black cycle
        s_job_cycle++;
        if (s_job_cycle < s_totalCycles) {
          s_job_phase = PHASE_WHITE;
        } else {
          s_job_phase = PHASE_FINAL_WHITE;
        }
      }
    }
    return;
  }

  // We're mid-page sequence: draw the next page for current pass
  display.fillScreen(drawColor);
  bool more = display.nextPage();
  if (more) {
    // still more pages to draw for this pass; remain in page mode
    s_job_inPage = true;
    return;
  }

  // Finished the pass
  s_job_inPage = false;
  s_job_lastStepMs = millis();
  if (s_job_phase == PHASE_WHITE) {
    s_job_phase = PHASE_BLACK;
  } else if (s_job_phase == PHASE_BLACK) {
    s_job_cycle++;
    if (s_job_cycle < s_totalCycles) {
      s_job_phase = PHASE_WHITE;
    } else {
      s_job_phase = PHASE_FINAL_WHITE;
    }
  } else if (s_job_phase == PHASE_FINAL_WHITE) {
    // final white pass completed -> finish job
    Serial.println("Background clear done (incremental)");
    if (oled_isAvailable()) oled_showStatus("Cleared");
    s_job_active = false;
    s_busy = false;
  }
}
