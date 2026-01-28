/*
  display.cpp

  Implementation of the e-paper display helper API.
  Refactored to use a background FreeRTOS task to ensure the system remains responsive
  during long E-Ink refresh cycles.
*/

#include "display.h"
#include "config.h"
#include "utils/base64.h"
#include "drivers/oled/oled.h"

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <LittleFS.h>

#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// --- Types ---
enum epd_job_type_t {
  JOB_TEXT,
  JOB_IMAGE,
  JOB_CLEAR,
  JOB_FORCE_CLEAR,
  JOB_DATE,
  JOB_HEADER,
  JOB_PAGE
};

#include "layout.h"

struct epd_job_t {
  epd_job_type_t type;
  String text;
  uint16_t color;
  bool forceFull;
  int width;
  int height;
  std::vector<uint8_t> data;
  String format;
  String imageColor;
  time_t time;
  EpdPage page;
};

// --- Hardware ---
static GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> display(GxEPD2_290_T94_V2(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));
static U8G2_FOR_ADAFRUIT_GFX s_u8g2_epd;

// --- State ---
static String g_currentText = "Hello API";
static uint16_t g_currentColor = GxEPD_BLACK;
static bool g_partialEnabled = ENABLE_PARTIAL_UPDATE;
static volatile bool s_isBlockedByTask = false;

// --- Task & Queue ---
static TaskHandle_t s_epdTaskHandle = NULL;
static QueueHandle_t s_jobQueue = NULL;
static SemaphoreHandle_t s_stateMutex = NULL;

// Internal execution helpers (called from task)
static void _exec_displayText(const epd_job_t &job);
static void _exec_displayHeader(const epd_job_t &job);
static void _exec_drawImage(const epd_job_t &job);
static void _exec_displayPage(const epd_job_t &job);
static void _exec_clear(bool force);

// Background task worker
static void epd_worker_task(void *pvParameters) {
  Serial.println("EPD Task: started");
  
  while (true) {
    epd_job_t *job = nullptr;
    // Wait for a job to arrive in the queue
    if (xQueueReceive(s_jobQueue, &job, portMAX_DELAY) == pdPASS && job != nullptr) {
      s_isBlockedByTask = true;
      
      switch (job->type) {
        case JOB_TEXT:
          _exec_displayText(*job);
          break;
        case JOB_IMAGE:
          _exec_drawImage(*job);
          break;
        case JOB_CLEAR:
          _exec_clear(false);
          break;
        case JOB_FORCE_CLEAR:
          _exec_clear(true);
          break;
        case JOB_DATE:
          // Display date in DD.MM format at bottom-right (same as wallpaper overlay)
          {
            struct tm tm;
            localtime_r(&job->time, &tm);
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d.%02d", tm.tm_mday, tm.tm_mon + 1);
            
            // Create image job with date text overlay (empty image, just date)
            epd_job_t dateJob = *job;
            dateJob.type = JOB_IMAGE;
            dateJob.width = display.width();
            dateJob.height = display.height();
            dateJob.data.clear(); // Empty image data
            dateJob.format = "empty"; // Special marker for empty image
            dateJob.imageColor = "black";
            dateJob.forceFull = true;
            dateJob.text = String(buf);
            _exec_drawImage(dateJob);
          }
          break;
        case JOB_HEADER:
          _exec_displayHeader(*job);
          break;
        case JOB_PAGE:
          _exec_displayPage(*job);
          break;
      }
      
      delete job;
      s_isBlockedByTask = false;
    }
  }
}

void epd_init() {
  // Initialize SPI
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

  // Initialize OLED early
  oled_init(PIN_OLED_SDA, PIN_OLED_SCL, OLED_I2C_ADDR);
  if (oled_isAvailable()) {
    oled_showStatus("EPD init...");
  }

  // Init e-paper display hardware
  display.init(115200, false, 50, false);
  
  // Init U8g2 helper
  s_u8g2_epd.begin(display);
  s_u8g2_epd.setFont(u8g2_font_profont29_tr);
  s_u8g2_epd.setForegroundColor(GxEPD_BLACK);
  s_u8g2_epd.setBackgroundColor(GxEPD_WHITE);

  // Sync state mutex
  s_stateMutex = xSemaphoreCreateMutex();
  
  // Create job queue (limit to 5 pending jobs to avoid memory exhaustion)
  s_jobQueue = xQueueCreate(5, sizeof(epd_job_t *));
  
  // Start the background task
  // Priority slightly lower than main loop to favor UI/Net responsiveness
  xTaskCreate(epd_worker_task, "epd_task", 8192, NULL, 1, &s_epdTaskHandle);

  if (oled_isAvailable()) {
    oled_showStatus("Ready");
  }
}

// Queue a job safely
static bool _queueJob(epd_job_t *job) {
  if (s_jobQueue == NULL || job == NULL) return false;
  
  // Try to send to queue. If full, we fail.
  // We use 0 wait time to avoid blocking the caller.
  if (xQueueSend(s_jobQueue, &job, 0) != pdPASS) {
    Serial.println("EPD: Job queue full, skipping request");
    delete job;
    return false;
  }
  return true;
}

void epd_displayText(const String &txt, uint16_t color, bool forceFull) {
  if (txt.length() == 0) return;
  
  epd_job_t *job = new epd_job_t();
  job->type = JOB_TEXT;
  job->text = txt;
  job->color = color;
  job->forceFull = forceFull;
  
  _queueJob(job);
}

void epd_displayHeader(const String &txt) {
    if (txt.length() == 0) return;
    epd_job_t *job = new epd_job_t();
    job->type = JOB_HEADER;
    job->text = txt;
    _queueJob(job);
}

void epd_displayDate(time_t now) {
  epd_job_t *job = new epd_job_t();
  job->type = JOB_DATE;
  job->time = now;
  
  _queueJob(job);
}

void epd_displayWallpaper(time_t now) {
  int width = 128;
  int height = 296;
  std::vector<uint8_t> data;
  
  // Check if custom wallpaper exists
  if (LittleFS.exists("/wallpaper.bin")) {
    File f = LittleFS.open("/wallpaper.bin", "r");
    if (f) {
      // Read header
      uint8_t header[4];
      if (f.read(header, 4) == 4) {
        width = (header[0] << 8) | header[1];
        height = (header[2] << 8) | header[3];
        
        // Read bitmap data
        size_t dataSize = f.size() - 4;
        data.resize(dataSize);
        if (f.read(data.data(), dataSize) == dataSize) {
          f.close();
          goto display_wallpaper; // Successfully loaded custom wallpaper
        }
      }
      f.close();
    }
  }
  
  // Generate default noisy wallpaper
  {
    int bytesPerRow = (width + 7) / 8;
    size_t totalBytes = bytesPerRow * height;
    data.resize(totalBytes);
    
    // Generate random noise pattern with fixed seed for consistency
    randomSeed(42);
    for (size_t i = 0; i < totalBytes; i++) {
      data[i] = random(256);
    }
  }
  
display_wallpaper:
  // Prepare date string in DD.MM format
  struct tm tm;
  localtime_r(&now, &tm);
  char dateBuf[16];
  snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d", tm.tm_mday, tm.tm_mon + 1);
  
  // Queue a job to draw wallpaper + date
  epd_job_t *job = new epd_job_t();
  job->type = JOB_IMAGE;
  job->width = width;
  job->height = height;
  job->data = data;
  job->format = "bw";
  job->imageColor = "black";
  job->forceFull = true;
  job->text = String(dateBuf); // Store date in text field for overlay
  
  _queueJob(job);
}

void epd_clear() {
  epd_job_t *job = new epd_job_t();
  job->type = JOB_CLEAR;
  _queueJob(job);
}

void epd_forceClear() {
  epd_job_t *job = new epd_job_t();
  job->type = JOB_FORCE_CLEAR;
  _queueJob(job);
}

bool epd_forceClear_async() {
  return _queueJob(new epd_job_t{JOB_FORCE_CLEAR});
}

bool epd_drawImageFromBitplanes(int width, int height, const std::vector<uint8_t> &data, const char *format, const char *color, bool forceFull) {
  epd_job_t *job = new epd_job_t();
  job->type = JOB_IMAGE;
  job->width = width;
  job->height = height;
  job->data = data; // copies vector
  job->format = String(format);
  job->imageColor = String(color);
  job->forceFull = forceFull;
  
  return _queueJob(job);
}

void epd_displayPage(const EpdPage& page) {
    epd_job_t *job = new epd_job_t();
    job->type = JOB_PAGE;
    job->page = page;
    _queueJob(job);
}

bool epd_isBusy() {
  if (s_jobQueue == NULL) return false;
  return s_isBlockedByTask || (uxQueueMessagesWaiting(s_jobQueue) > 0);
}

void epd_runBackgroundJobs() {
  // Now handled by the task; this is kept for API compatibility.
  // We can use it to yield or do nothing.
}

// Internal State Access (Thread-safe-ish since we read from main which usually is the one setting it via queue)
String epd_getCurrentText() { return g_currentText; }
uint16_t epd_width() { return display.width(); }
uint16_t epd_height() { return display.height(); }
bool epd_hasPartialUpdate() { return display.epd2.hasPartialUpdate; }
void epd_setPartialEnabled(bool enabled) { g_partialEnabled = enabled; }
bool epd_getPartialEnabled() { return g_partialEnabled; }

// --- Implementation of rendering (runs in task) ---

static void _exec_displayText(const epd_job_t &job) {
  g_currentText = job.text;
  if (oled_isAvailable()) oled_showStatus("Rendering...");

  s_u8g2_epd.setFont(u8g2_font_profont29_tr);
  s_u8g2_epd.setBackgroundColor(GxEPD_WHITE);
  s_u8g2_epd.setForegroundColor(job.color);

  int16_t bw = s_u8g2_epd.getUTF8Width(job.text.c_str());
  int16_t bh = s_u8g2_epd.getFontAscent() - s_u8g2_epd.getFontDescent();

  if ((int)bw > (int)(display.width() - 8)) {
    s_u8g2_epd.setFont(u8g2_font_profont17_tr);
    bw = s_u8g2_epd.getUTF8Width(job.text.c_str());
    bh = s_u8g2_epd.getFontAscent() - s_u8g2_epd.getFontDescent();
  }

  int16_t cx = (display.width() - bw) / 2;
  int16_t cy = (display.height() / 2) + (s_u8g2_epd.getFontAscent() / 2);

  const int pad = 4;
  int16_t rx = ((display.width() - bw) / 2) - pad;
  int16_t ry = ((display.height() - bh) / 2) - pad;
  uint16_t rw = bw + pad * 2;
  uint16_t rh = bh + pad * 2;

  // Clamp
  rx = std::max((int16_t)0, rx);
  ry = std::max((int16_t)0, ry);
  rw = std::min((uint16_t)(display.width() - rx), rw);
  rh = std::min((uint16_t)(display.height() - ry), rh);

  bool usedPartial = false;
  if (g_partialEnabled && display.epd2.hasPartialUpdate && !job.forceFull) {
    usedPartial = true;
    display.setPartialWindow(rx, ry, rw, rh);
  } else {
    display.setFullWindow();
  }

  display.firstPage();
  do {
    if (usedPartial) display.fillRect(rx, ry, rw, rh, GxEPD_WHITE);
    else display.fillScreen(GxEPD_WHITE);
    s_u8g2_epd.setCursor(cx, cy);
    s_u8g2_epd.print(job.text);
  } while (display.nextPage());

  if (oled_isAvailable()) oled_showStatus("Done");
}

static void _exec_displayHeader(const epd_job_t &job) {
  if (oled_isAvailable()) oled_showStatus("EPD Header...");

  s_u8g2_epd.setFont(u8g2_font_profont17_tr);
  s_u8g2_epd.setBackgroundColor(GxEPD_WHITE);
  s_u8g2_epd.setForegroundColor(GxEPD_BLACK);

  int16_t bh = s_u8g2_epd.getFontAscent() - s_u8g2_epd.getFontDescent();
  uint16_t rw = display.width();
  uint16_t rh = bh + 8;

  display.setPartialWindow(0, 0, rw, rh);
  display.firstPage();
  do {
    display.fillRect(0, 0, rw, rh, GxEPD_BLACK);
    s_u8g2_epd.setForegroundColor(GxEPD_WHITE);
    s_u8g2_epd.setCursor(8, s_u8g2_epd.getFontAscent() + 4);
    s_u8g2_epd.print(job.text);
  } while (display.nextPage());

  if (oled_isAvailable()) oled_showStatus("Done");
}

static void _exec_image_bw(int width, int height, const std::vector<uint8_t> &img, int rx, int ry, const char *color) {
  int bytesPerRow = (width + 7) / 8;
  uint16_t drawColor = GxEPD_BLACK;

  for (int y = 0; y < height; ++y) {
    int row = y * bytesPerRow;
    for (int x = 0; x < width; ++x) {
      int byteIndex = row + (x >> 3);
      uint8_t b = img[byteIndex];
      bool pixelOn = ((b >> (7 - (x & 7))) & 1);
      if (pixelOn) display.drawPixel(rx + x, ry + y, drawColor);
    }
  }
}

// 3c image handling removed as we are now BW only


static void _exec_drawImage(const epd_job_t &job) {
  if (oled_isAvailable()) oled_showStatus("Loading...");

  int rx = ((int)display.width() - job.width) / 2;
  int ry = ((int)display.height() - job.height) / 2;

  bool usedPartial = false;
  if (g_partialEnabled && display.epd2.hasPartialUpdate && !job.forceFull) {
    usedPartial = true;
    display.setPartialWindow(rx, ry, job.width, job.height);
  } else {
    display.setFullWindow();
  }

  display.firstPage();
  do {
    if (usedPartial) display.fillRect(rx, ry, job.width, job.height, GxEPD_WHITE);
    else display.fillScreen(GxEPD_WHITE);

    // Only draw image if there's actual image data
    if (job.format != "empty") {
       // Always use BW handler, ignoring format/color args for now since we are BW only
       _exec_image_bw(job.width, job.height, job.data, rx, ry, "black");
    }
    
    // If text is present (date overlay for wallpaper), draw it at the bottom
    if (job.text.length() > 0) {
      s_u8g2_epd.setFont(u8g2_font_profont17_tr);
      s_u8g2_epd.setForegroundColor(GxEPD_BLACK);
      s_u8g2_epd.setBackgroundColor(GxEPD_WHITE);
      
      int16_t tw = s_u8g2_epd.getUTF8Width(job.text.c_str());
      int16_t th = s_u8g2_epd.getFontAscent() - s_u8g2_epd.getFontDescent();
      
      // Bottom-right position with small margin
      int16_t text_x = display.width() - tw - 4;
      int16_t text_y = display.height() - 4;
      
      // Draw white background box for date
      display.fillRect(text_x - 2, text_y - th - 2, tw + 4, th + 4, GxEPD_WHITE);
      
      // Draw text
      s_u8g2_epd.setCursor(text_x, text_y);
      s_u8g2_epd.print(job.text);
    }
  } while (display.nextPage());

  if (oled_isAvailable()) oled_showStatus("Done");
}

static void _exec_displayPage(const epd_job_t &job) {
    if (oled_isAvailable()) oled_showStatus("EPD Layout...");

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        int16_t currY = 0;
        
        // 1. Draw Header
        if (job.page.title.length() > 0) {
            s_u8g2_epd.setFont(u8g2_font_profont15_tr);
            s_u8g2_epd.setFontMode(1); // Transparent mode
            s_u8g2_epd.setForegroundColor(GxEPD_BLACK);
            
            // Normal
            s_u8g2_epd.setCursor(2, s_u8g2_epd.getFontAscent() + 4); // X=2 aligned
            s_u8g2_epd.print(job.page.title);
            
            // Bold (Offset +1)
            s_u8g2_epd.setCursor(3, s_u8g2_epd.getFontAscent() + 4);
            s_u8g2_epd.print(job.page.title);
            
            currY = s_u8g2_epd.getFontAscent() + 8;
            display.drawFastHLine(0, currY, display.width(), GxEPD_BLACK);
            display.drawFastHLine(0, currY + 1, display.width(), GxEPD_BLACK); // Double line
            currY += 10; // Slightly reduced margin from 12->10
        } else {
            currY = 2; // Minimal top margin if no title
        }

        // 2. Draw Components
        for (const auto& comp : job.page.components) {
            if (currY > display.height() - 12) break;

            s_u8g2_epd.setFontMode(1);
            
            switch (comp.type) {
                case EPD_COMP_HEADER:
                    s_u8g2_epd.setFont(u8g2_font_profont15_tr);
                    s_u8g2_epd.setForegroundColor(GxEPD_BLACK);
                    
                    // Draw Normal
                    s_u8g2_epd.setCursor(2, currY + s_u8g2_epd.getFontAscent()); 
                    s_u8g2_epd.print(comp.text1);
                    
                    // Draw Bold (Offset +1)
                    s_u8g2_epd.setCursor(3, currY + s_u8g2_epd.getFontAscent()); 
                    s_u8g2_epd.print(comp.text1);
                    
                    currY += (s_u8g2_epd.getFontAscent() - s_u8g2_epd.getFontDescent()) + 1;
                    break;

                case EPD_COMP_ROW:
                    s_u8g2_epd.setFont(u8g2_font_profont15_tr);
                    s_u8g2_epd.setForegroundColor(GxEPD_BLACK);
                    
                    // Label (Left)
                    s_u8g2_epd.setCursor(2, currY + s_u8g2_epd.getFontAscent()); // X changed from 8 to 2
                    s_u8g2_epd.print(comp.text1);
                    
                    // Value (Right)
                    if (comp.text2.length() > 0) {
                        s_u8g2_epd.setForegroundColor(comp.color == 0 ? GxEPD_BLACK : comp.color);
                        int16_t valW = s_u8g2_epd.getUTF8Width(comp.text2.c_str());
                        s_u8g2_epd.setCursor(display.width() - valW - 2, currY + s_u8g2_epd.getFontAscent()); // Right margin 2
                        s_u8g2_epd.print(comp.text2);
                    }
                    currY += 12;
                    break;

                case EPD_COMP_PROGRESS:
                    s_u8g2_epd.setFont(u8g2_font_profont12_tr);
                    s_u8g2_epd.setForegroundColor(GxEPD_BLACK);
                    s_u8g2_epd.setCursor(2, currY + s_u8g2_epd.getFontAscent()); // X changed from 8 to 2
                    s_u8g2_epd.print(comp.text1);
                    
                    {
                        int16_t barW = display.width() - 100;
                        int16_t barX = display.width() - barW - 30; // Adjusted for new spacing
                        int16_t barY = currY + 2;
                        int16_t barH = 8;
                        display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
                        int16_t fillW = (int16_t)((barW - 4) * (comp.value / 100.0f));
                        if (fillW > 0) {
                            display.fillRect(barX + 2, barY + 2, fillW, barH - 4, comp.color == 0 ? GxEPD_BLACK : comp.color);
                        }
                        
                        // Percentage text
                        s_u8g2_epd.setCursor(display.width() - 25, currY + s_u8g2_epd.getFontAscent()); // Adjusted margin
                        s_u8g2_epd.print(comp.text2);
                    }
                    currY += 14;
                    break;

                case EPD_COMP_SEPARATOR:
                    display.drawFastHLine(2, currY + 1, display.width() - 4, GxEPD_BLACK); // X=2, Width-4
                    currY += 4;
                    break;
            }
        }
    } while (display.nextPage());

    if (oled_isAvailable()) oled_showStatus("Done");
}

static void _exec_clear(bool force) {
  if (oled_isAvailable()) oled_showStatus(force ? "Recovery..." : "Clearing...");
  
  if (force) {
    const int cycles = 4;
    for (int i = 0; i < cycles; ++i) {
      if (oled_isAvailable()) oled_showProgress("Clearing", i + 1, cycles);
      display.setFullWindow(); display.firstPage();
      do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
      vTaskDelay(pdMS_TO_TICKS(400));
      display.setFullWindow(); display.firstPage();
      do { display.fillScreen(GxEPD_BLACK); } while (display.nextPage());
      vTaskDelay(pdMS_TO_TICKS(400));
    }
    display.setFullWindow(); display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
    vTaskDelay(pdMS_TO_TICKS(200));
  } else {
    display.setFullWindow();
    display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
  }
  
  if (oled_isAvailable()) oled_showStatus("Cleared");
}

