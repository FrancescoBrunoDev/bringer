#include "epub.h"
#include "../ui/ui.h"
#include "drivers/oled/oled.h"
#include "drivers/epaper/display.h"
#include "drivers/epaper/layout.h"
#include "app/ui/common/types.h"
#include "utils/logger/logger.h"
#include <LittleFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <GxEPD2_3C.h>

// Zip library removed until valid one found
// #include <ESP32-targz.h> 

using namespace std;

// --- State ---
static struct {
    vector<String> bookList;
    int bookIndex = 0;
    int prevBookIndex = 0;
    
    // Current Book
    String currentBookPath;
    String currentTitle;
    vector<String> spine; // List of HTML paths in order
    int chapterIndex = 0;
    int prevChapterIndex = 0;
    
    // Current Chapter
    String currentChapterText; // Stripped text
    int pageIndex = 0;
    int totalPages = 1;
    
    bool isLoading = false;
} s_state;

// --- Helper Prototypes ---
static void loadBookList();
static bool indexBook(const String& path); // Parse OPF/Spine
static void loadChapter(int index);
static void renderRead(int16_t x, int16_t y);
static void renderPage();
static void stripTags(String& html);
static void saveProgress();
static void loadProgress();

// --- Views ---

// 1. Book List View
static void render_book_item(int index, int16_t x, int16_t y) {
    if (index < 0 || index >= s_state.bookList.size()) return;
    String title = s_state.bookList[index];
    int lastSlash = title.lastIndexOf('/');
    if (lastSlash >= 0) title = title.substring(lastSlash + 1);
    if (title.endsWith(".epub")) title = title.substring(0, title.length() - 5);
    
    oled_drawBigText(title.c_str(), x, y, false, true);
}

static void renderBookList(int16_t x, int16_t y) {
    if (s_state.bookList.empty()) {
        oled_drawBigText("No Books", x, y, false);
        return;
    }
    
    if (abs(y) < 1) {
        render_book_item(s_state.bookIndex, x, 0);
    } else {
        render_book_item(s_state.bookIndex, x, y);
        render_book_item(s_state.prevBookIndex, x, y > 0 ? y - 64 : y + 64);
    }
    
    // Do not show progress bar during transition to keep it clean, or show stationary?
    // Settings doesn't show progress bar. We can show it at bottom if y==0
    if (abs(y) < 1) {
         char buf[32];
         snprintf(buf, sizeof(buf), "Book %d/%d", s_state.bookIndex + 1, s_state.bookList.size());
         oled_drawHeader(buf, x, 52); // Draw as footer
    }
}

static void onBookListNext() {
    if (s_state.bookList.empty()) return;
    s_state.prevBookIndex = s_state.bookIndex;
    s_state.bookIndex = (s_state.bookIndex + 1) % s_state.bookList.size();
    ui_triggerVerticalAnimation(true);
}

static void onBookListPrev() {
    if (s_state.bookList.empty()) return;
    s_state.prevBookIndex = s_state.bookIndex;
    s_state.bookIndex = (s_state.bookIndex + s_state.bookList.size() - 1) % s_state.bookList.size();
    ui_triggerVerticalAnimation(false);
}

static void onBookListBack() {
    // Explicitly exit to carousel, ensuring clean state
    ui_setView(NULL);
}

// Forward declare
extern const View viewRead;

static void onBookListSelect() {
    if (s_state.bookList.empty()) return;
    
    s_state.currentBookPath = s_state.bookList[s_state.bookIndex];
    oled_showStatus("Opening...");
    
    if (indexBook(s_state.currentBookPath)) {
        loadProgress(); // Restore last position
        loadChapter(s_state.chapterIndex);
        ui_setView(&viewRead);
    } else {
        oled_showStatus("Error");
        delay(1000);
    }
}

static const View viewBookList = {
    .title = "Select Book",
    .render = renderBookList,
    .onNext = onBookListNext,
    .onPrev = onBookListPrev,
    .onSelect = onBookListSelect,
    .onBack = onBookListBack, 
    .poll = NULL,
    .getScrollProgress = []() -> float { 
        if(s_state.bookList.empty()) return 0.0f;
        return (float)(s_state.bookIndex + 1) / (float)s_state.bookList.size();
    }
};

// 2. Read View (Main Reader)


static void updateEpaper() {
    if (s_state.currentChapterText.length() == 0) {
        epd_displayText("Empty Chapter", 0);
        return;
    }
    
    // Display: 296x128 pixels (VERTICAL: 296 height, 128 width)
    // Using profont12 (~6x10): 128px / 6 = ~21 chars/line, 296px / 10 = ~29 lines
    // But EPD_COMP_ROW has margins, so reduce both. 19 chars to avoid clipping.
    const int CHARS_PER_LINE = 19;
    const int LINES_PER_PAGE = 24;
    const int CHARS_PER_PAGE = CHARS_PER_LINE * LINES_PER_PAGE;
    
    int start = s_state.pageIndex * CHARS_PER_PAGE;
    if (start >= s_state.currentChapterText.length()) {
        start = 0;
        s_state.pageIndex = 0;
    }
    
    // Extract text for this page
    int end = start + CHARS_PER_PAGE;
    if (end < s_state.currentChapterText.length()) {
        // Try to break at sentence end
        int sentenceEnd = s_state.currentChapterText.lastIndexOf('.', end);
        if (sentenceEnd > start && sentenceEnd - start > CHARS_PER_PAGE * 0.7) {
            end = sentenceEnd + 1;
        } else {
            // Break at word boundary
            int spacePos = s_state.currentChapterText.lastIndexOf(' ', end);
            if (spacePos > start && spacePos - start > CHARS_PER_PAGE * 0.5) {
                end = spacePos;
            }
        }
    } else {
        end = s_state.currentChapterText.length();
    }
    
    String pageText = s_state.currentChapterText.substring(start, end);
    pageText.trim();
    
    // Create EpdPage with text content as multiple rows
    EpdPage page;
    page.title = "";
    
    // Split text into lines manually
    int pos = 0;
    int lineCount = 0;
    while (pos < pageText.length() && lineCount < LINES_PER_PAGE) {
        int lineEnd = pos + CHARS_PER_LINE;
        String line;
        
        if (lineEnd >= pageText.length()) {
            line = pageText.substring(pos);
            pos = pageText.length();
        } else {
            // Find last space before lineEnd
            int spacePos = pageText.lastIndexOf(' ', lineEnd);
            if (spacePos > pos && spacePos - pos > CHARS_PER_LINE * 0.6) {
                line = pageText.substring(pos, spacePos);
                pos = spacePos + 1;
            } else {
                line = pageText.substring(pos, lineEnd);
                pos = lineEnd;
            }
        }
        
        // Add line as a simple row component (using text1 only, no text2)
        EpdComponent comp;
        comp.type = EPD_COMP_ROW;
        comp.text1 = line;
        comp.text2 = "";
        comp.value = 0;
        comp.color = GxEPD_BLACK;
        page.components.push_back(comp);
        lineCount++;
    }
    
    epd_displayPage(page);
}

static void onReadNext() {
    if (s_state.pageIndex < s_state.totalPages - 1) {
        s_state.pageIndex++;
        updateEpaper();
    } else {
        // Next chapter
        if (s_state.chapterIndex < s_state.spine.size() - 1) {
            s_state.chapterIndex++;
            oled_showStatus("Loading...");
            loadChapter(s_state.chapterIndex);
            saveProgress(); // checkpoints
        }
    }
}

static void onReadPrev() {
    if (s_state.pageIndex > 0) {
        s_state.pageIndex--;
        updateEpaper();
    } else {
        // Prev chapter
        if (s_state.chapterIndex > 0) {
            s_state.chapterIndex--;
            oled_showStatus("Loading...");
            loadChapter(s_state.chapterIndex);
            // Go to last page of new chapter?
            // For now start at 0
            saveProgress();
        }
    }
}

// Forward declare chapter selection
extern const View viewChapterList;

static void onReadSelect() {
    // Open chapter menu
    ui_setView(&viewChapterList);
}

const View viewRead = {
    .title = NULL, // Managed manually
    .render = renderRead,
    .onNext = onReadNext,
    .onPrev = onReadPrev,
    .onSelect = onReadSelect,
    .onBack = [](){ saveProgress(); ui_setView(&viewBookList); }, // Back to book list
    .poll = NULL,
    .getScrollProgress = []() -> float { 
        if (s_state.totalPages <= 1) return 0.0f;
        return (float)s_state.pageIndex / (float)(s_state.totalPages - 1);
    }
};

static void render_chapter_item(int index, int16_t x, int16_t y) {
    if(index < 0 || index >= s_state.spine.size()) return;
    String name = s_state.spine[index];
    // Strip path
    int idx = name.lastIndexOf('/');
    if (idx >= 0) name = name.substring(idx+1);
    
    oled_drawBigText(name.c_str(), x, y, false, true);
}

// 3. Chapter List View
static void renderChapterList(int16_t x, int16_t y) {
    if (abs(y) < 1) {
         oled_drawHeader("Select Chapter", x, y);
         render_chapter_item(s_state.chapterIndex, x, 16);
    } else {
         render_chapter_item(s_state.chapterIndex, x, y + 16);
         render_chapter_item(s_state.prevChapterIndex, x, y > 0 ? (y + 16) - 64 : (y + 16) + 64);
    }
}

static void onChapterNext() {
    if (s_state.spine.empty()) return;
    s_state.prevChapterIndex = s_state.chapterIndex;
    s_state.chapterIndex = (s_state.chapterIndex + 1) % s_state.spine.size();
    ui_triggerVerticalAnimation(true);
}

static void onChapterPrev() {
    if (s_state.spine.empty()) return;
    s_state.prevChapterIndex = s_state.chapterIndex;
    s_state.chapterIndex = (s_state.chapterIndex + s_state.spine.size() - 1) % s_state.spine.size();
    ui_triggerVerticalAnimation(false);
}

static void onChapterSelect() {
    oled_showStatus("Loading...");
    loadChapter(s_state.chapterIndex);
    saveProgress();
    ui_setView(&viewRead);
}

const View viewChapterList = {
    .title = "Select Chapter",
    .render = renderChapterList,
    .onNext = onChapterNext,
    .onPrev = onChapterPrev,
    .onSelect = onChapterSelect,
    .onBack = [](){ ui_setView(&viewRead); }, // Back to read
    .poll = NULL,
    .getScrollProgress = []() -> float {
        if (s_state.spine.empty()) return 0.0f;
        return (float)s_state.chapterIndex / (float)s_state.spine.size();
    }
};

// --- App Interface ---

static void app_renderPreview(int16_t x, int16_t y) {
    oled_drawBigText("Epub Reader", x, y, false);
}

static void app_onSelect() {
    loadBookList();
    ui_setView(&viewBookList);
}

const App APP_EPUB = {
    .name = "Epub Reader",
    .renderPreview = app_renderPreview,
    .onSelect = app_onSelect,
    .poll = NULL
};

// --- Helpers Implementation ---

static void loadBookList() {
    s_state.bookList.clear();
    // Use LittleFS to list /epubs
    if (!LittleFS.exists("/epubs")) LittleFS.mkdir("/epubs");
    
    File dir = LittleFS.open("/epubs");
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();
    while(file){
        String name = file.name(); // usually returns full path or name? LittleFS returns name relative to dir in some versions, or full.
        // name check
        if (name.endsWith(".epub")) {
             // ensure path
             String fullPath = String("/epubs/") + name;
             if (name.startsWith("/")) fullPath = name; // already absolute
             s_state.bookList.push_back(fullPath);
        }
        file = dir.openNextFile();
    }
    s_state.bookIndex = 0;
}

// Logic to parsing EPUB
// Simplified: We assume we unpacked the EPUB to a temp directory or we just extract the specific files we need.
// Since 'ESP32-targz' unzips everything, we will try to keep it simple: 
// On "Open": Unzip useful files (html, opf) to /tmp_book/, ignoring others.
// Warning: This is slow. 
// Optimization: Check if /tmp_book/ is already this book?
// MOCK REMOVED - Real ZIP Scanner
// Better ZIP Scanner: Read Central Directory
static bool indexBook(const String& path) {
    s_state.spine.clear();
    s_state.currentTitle = path.substring(path.lastIndexOf('/')+1);
    if(s_state.currentTitle.endsWith(".epub")) 
        s_state.currentTitle = s_state.currentTitle.substring(0, s_state.currentTitle.length()-5);

    File f = LittleFS.open(path, "r");
    if (!f) {
        logger_log("EPUB: Failed to open %s", path.c_str());
        return false;
    }

    size_t fileSize = f.size();
    if (fileSize < 22) { f.close(); return false; }

    // Find EOCD
    // Scan last 4KB (or file size)
    size_t scanSize = (fileSize > 4096) ? 4096 : fileSize;
    size_t scanStart = fileSize - scanSize;
    
    uint8_t buf[1024]; // Process in chunks
    // We need to find signature 0x50 0x4B 0x05 0x06
    // It is 4 bytes.
    
    uint32_t eocdPos = 0;
    bool found = false;
    
    // Simple scan backwards?
    // Let's just read the whole tail into a buffer if fits, or chunks logic.
    // For simplicity on ESP32, let's just seek and read byte by byte backwards from end? slow.
    // Let's read 1KB blocks from end.
    
    // Actually, just reading the last 512 bytes is usually enough if no huge comment.
    f.seek(scanStart);
    // Read entirely into heap? 4KB is fine.
    uint8_t* scanBuf = (uint8_t*)malloc(scanSize);
    if(!scanBuf) { f.close(); return false; }
    
    f.read(scanBuf, scanSize);
    
    // Scan backwards
    for (int i = scanSize - 4; i >= 0; i--) {
        if (scanBuf[i] == 0x50 && scanBuf[i+1] == 0x4B && scanBuf[i+2] == 0x05 && scanBuf[i+3] == 0x06) {
            eocdPos = scanStart + i;
            found = true;
            break;
        }
    }
    
    free(scanBuf);
    
    if (!found) {
        logger_log("EPUB: EOCD not found");
        f.close();
        return false; // Not a valid zip or huge comment
    }
    
    // Read EOCD fields
    f.seek(eocdPos + 10);
    uint16_t totalEntries = f.read() | (f.read() << 8);
    // uint32_t cdSize = f.read() | (f.read() << 8) | (f.read() << 16) | (f.read() << 24);
    f.seek(eocdPos + 16);
    uint32_t cdOffset = f.read() | (f.read() << 8) | (f.read() << 16) | (f.read() << 24);
    
    logger_log("EPUB: CD Offset %d, Entries %d", cdOffset, totalEntries);
    
    // Go to Central Directory
    f.seek(cdOffset);
    
    for (int i = 0; i < totalEntries; i++) {
        if (f.position() >= fileSize) break;
        
        // Read CD Header Signature
        uint8_t sig[4];
        if (f.read(sig, 4) != 4) break;
        if (!(sig[0] == 0x50 && sig[1] == 0x4B && sig[2] == 0x01 && sig[3] == 0x02)) {
             break; // Invalid sig
        }
        
        f.seek(f.position() + 24); // Skip to name len
        uint16_t nameLen = f.read() | (f.read() << 8);
        uint16_t extraLen = f.read() | (f.read() << 8);
        uint16_t commentLen = f.read() | (f.read() << 8);
        
        f.seek(f.position() + 12); // Skip rest of header to filename
        
        String filename = "";
        for(int k=0; k<nameLen; k++) filename += (char)f.read();
        
        // Filter
        if (filename.endsWith(".html") || filename.endsWith(".xhtml") || filename.endsWith(".htm")) {
             logger_log("EPUB: Adding chapter: %s", filename.c_str());
             s_state.spine.push_back(filename);
        }
        
        // Skip extra and comment
        f.seek(f.position() + extraLen + commentLen);
        
        // Yield to watchdog if needed?
        if (i % 10 == 0) yield();
    }
    
    f.close();
    
    // Sort? CD usually reflects order, but sometimes alphabetical.
    // Ideally parse OPF. But for now, this is better than broken naive scan.
    std::sort(s_state.spine.begin(), s_state.spine.end());
    
    logger_log("EPUB: Found %d chapters", s_state.spine.size());
    
    if (s_state.spine.empty()) return false;
    return true;
}

// 2. Read View (Main Reader)
static void renderRead(int16_t x, int16_t y) {
     // Display Chapter Title on OLED
     // If we have a chapter index, show that name.
     String chName = "Unknown";
     if (s_state.chapterIndex < s_state.spine.size()) {
         chName = s_state.spine[s_state.chapterIndex];
         // Clean up name (remove path)
         int idx = chName.lastIndexOf('/');
         if (idx >= 0) chName = chName.substring(idx+1);
     }
     
    // Combine info
    char line2[64];
    if (s_state.totalPages > 1)
        snprintf(line2, sizeof(line2), "Ch %d/%d  Pg %d/%d", s_state.chapterIndex + 1, s_state.spine.size(), s_state.pageIndex + 1, s_state.totalPages);
    else
        snprintf(line2, sizeof(line2), "Ch %d/%d", s_state.chapterIndex + 1, s_state.spine.size());
        
    oled_showLines(chName.c_str(), line2, x, y);
}

static void saveProgress() {
    if(s_state.currentBookPath.length() == 0) return;
    
    // Hash path for filename
    // Simple hash
    unsigned long hash = 5381;
    for(unsigned int i=0; i<s_state.currentBookPath.length(); i++) 
        hash = ((hash << 5) + hash) + s_state.currentBookPath.charAt(i);
        
    String p = "/progress/" + String(hash) + ".json";
    
    if(!LittleFS.exists("/progress")) LittleFS.mkdir("/progress");
    
    File f = LittleFS.open(p, "w");
    if(f) {
        StaticJsonDocument<128> doc;
        doc["chapter"] = s_state.chapterIndex;
        doc["page"] = s_state.pageIndex;
        serializeJson(doc, f);
        f.close();
    }
}

static void loadProgress() {
    if(s_state.currentBookPath.length() == 0) return;
     unsigned long hash = 5381;
    for(unsigned int i=0; i<s_state.currentBookPath.length(); i++) 
        hash = ((hash << 5) + hash) + s_state.currentBookPath.charAt(i);
        
    String p = "/progress/" + String(hash) + ".json";
    if(LittleFS.exists(p)) {
        File f = LittleFS.open(p, "r");
        if(f) {
             StaticJsonDocument<128> doc;
             deserializeJson(doc, f);
             s_state.chapterIndex = doc["chapter"] | 0;
             s_state.pageIndex = doc["page"] | 0;
             f.close();
             
             // Validate
             if(s_state.chapterIndex >= s_state.spine.size()) s_state.chapterIndex = 0;
        }
    }
}

// Use uzlib from ESP32-targz for decompression
#include <uzlib/uzlib.h>

// Helper to get raw data from ZIP based on filename
static bool getZipEntryData(const String& zipPath, const String& entryName, String& outContent) {
    File f = LittleFS.open(zipPath, "r");
    if (!f) return false;

    // We re-scan Central Directory to find the Local Header Offset
    
    // 1. Find EOCD
    size_t fileSize = f.size();
    if (fileSize < 22) { f.close(); return false; }
    size_t scanSize = (fileSize > 4096) ? 4096 : fileSize;
    size_t scanStart = fileSize - scanSize;
    f.seek(scanStart);
    uint8_t* scanBuf = (uint8_t*)malloc(scanSize);
    if (!scanBuf) { f.close(); return false; }
    f.read(scanBuf, scanSize);
    
    uint32_t eocdPos = 0;
    bool found = false;
    for (int i = scanSize - 4; i >= 0; i--) {
        if (scanBuf[i] == 0x50 && scanBuf[i+1] == 0x4B && scanBuf[i+2] == 0x05 && scanBuf[i+3] == 0x06) {
            eocdPos = scanStart + i;
            found = true;
            break;
        }
    }
    free(scanBuf);
    if (!found) { f.close(); return false; }
    
    // 2. Get CD Offset
    f.seek(eocdPos + 16);
    uint32_t cdOffset = f.read() | (f.read() << 8) | (f.read() << 16) | (f.read() << 24);
    f.seek(eocdPos + 10);
    uint16_t totalEntries = f.read() | (f.read() << 8);

    // 3. Scan CD for entry
    f.seek(cdOffset);
    uint32_t localHeaderOffset = 0;
    bool entryFound = false;
    uint32_t compSize = 0;
    uint32_t uncompSize = 0;
    uint16_t method = 0;
    
    logger_log("EPUB: Searching for '%s' in %d entries", entryName.c_str(), totalEntries);
    
    int debugCount = 0;
    for (int i = 0; i < totalEntries; i++) {
        if (f.position() >= fileSize) break;
        
        // Read CD Header Signature
        uint8_t sig[4];
        if (f.read(sig, 4) != 4) break;
        if (!(sig[0] == 0x50 && sig[1] == 0x4B && sig[2] == 0x01 && sig[3] == 0x02)) {
             break; // Invalid sig
        }
        
        // Read compression method first (at offset 10 from signature start)
        size_t sigPos = f.position() - 4; // Position of signature
        f.seek(sigPos + 10);
        method = f.read() | (f.read() << 8);
        
        // Read compressed/uncompressed sizes (at offset 20 from signature)
        f.seek(sigPos + 20);
        compSize = f.read() | (f.read() << 8) | (f.read() << 16) | (f.read() << 24);
        uncompSize = f.read() | (f.read() << 8) | (f.read() << 16) | (f.read() << 24);
        
        // Read name/extra/comment lengths (at offset 28 from signature)
        uint16_t nameLen = f.read() | (f.read() << 8);
        uint16_t extraLen = f.read() | (f.read() << 8);
        uint16_t commentLen = f.read() | (f.read() << 8);
        
        // Read local header offset (at offset 42 from signature)
        f.seek(sigPos + 42);
        localHeaderOffset = f.read() | (f.read() << 8) | (f.read() << 16) | (f.read() << 24);
        
        // Read filename (at offset 46 from signature)
        String fname = "";
        for(int k=0; k<nameLen; k++) fname += (char)f.read();
        
        // Log first few entries to see what we're actually reading
        if (debugCount < 5) {
            logger_log("EPUB:   Entry %d (len=%d): '%s'", i, nameLen, fname.c_str());
            debugCount++;
        }
        
        if (fname == entryName) {
            logger_log("EPUB: Found entry! method=%d, comp=%d, uncomp=%d", method, compSize, uncompSize);
            entryFound = true;
            break;
        }
        
        f.seek(f.position() + extraLen + commentLen);
    }
    
    if (!entryFound) { 
        logger_log("EPUB: Entry '%s' not found in archive", entryName.c_str());
        f.close(); 
        return false; 
    }
    
    // 4. Go to Local Header
    f.seek(localHeaderOffset);
    f.seek(f.position() + 26);
    uint16_t n = f.read() | (f.read() << 8);
    uint16_t m = f.read() | (f.read() << 8);
    f.seek(f.position() + n + m);
    
    // 5. Read Data
    if (uncompSize > 32*1024) uncompSize = 32*1024;
    if (compSize > 64*1024) { f.close(); return false; }

    if (method == 0) {
        // STORED
        if (compSize > 0) {
            uint8_t* raw = (uint8_t*)malloc(compSize + 1);
            if (raw) {
                f.read(raw, compSize);
                raw[compSize] = 0;
                outContent = (char*)raw;
                free(raw);
            }
        }
    } else if (method == 8) {
        // DEFLATE using uzlib
        logger_log("EPUB: Decompressing DEFLATE (comp:%d, uncomp:%d)", compSize, uncompSize);
        uint8_t* compData = (uint8_t*)malloc(compSize);
        if(!compData) { 
            logger_log("EPUB: Failed to alloc comp buffer");
            f.close(); 
            return false; 
        }
        
        f.read(compData, compSize);
        
        uint8_t* uncompData = (uint8_t*)malloc(uncompSize + 1);
        if(!uncompData) { 
            logger_log("EPUB: Failed to alloc uncomp buffer");
            free(compData); 
            f.close(); 
            return false; 
        }
        
        // Initialize uzlib decompressor
        TINF_DATA d;
        uzlib_init();
        d.source = compData;
        d.source_limit = compData + compSize;
        d.destStart = uncompData;
        d.dest = uncompData;
        
        // ZIP uses raw DEFLATE (no zlib/gzip headers)
        uzlib_uncompress_init(&d, NULL, 0);
        int res = uzlib_uncompress(&d);
        
        logger_log("EPUB: Decompress result: %d", res);
        
        if (res == TINF_DONE) {
            size_t actualSize = d.dest - uncompData;
            uncompData[actualSize] = 0;
            outContent = (char*)uncompData;
            logger_log("EPUB: Decompressed %d bytes", actualSize);
        } else {
            logger_log("EPUB: Decompression failed with code %d", res);
            outContent = "Decompression Failed";
        }
        
        free(uncompData);
        free(compData);
    }
    
    f.close();
    return true;
}

static void stripTags(String& html) {
    String result = "";
    result.reserve(html.length());
    
    bool inTag = false;
    bool inScript = false;
    bool inStyle = false;
    
    for (int i = 0; i < html.length(); i++) {
        char c = html.charAt(i);
        
        // Check for script/style tags
        if (c == '<' && i + 7 < html.length()) {
            String tag = html.substring(i, i + 8);
            tag.toLowerCase();
            if (tag.startsWith("<script")) inScript = true;
            if (tag.startsWith("<style")) inStyle = true;
        }
        if (c == '<' && i + 8 < html.length()) {
            String tag = html.substring(i, i + 9);
            tag.toLowerCase();
            if (tag == "</script>") inScript = false;
            if (tag == "</style>") inStyle = false;
        }
        
        if (c == '<') {
            inTag = true;
        } else if (c == '>') {
            inTag = false;
        } else if (!inTag && !inScript && !inStyle) {
            // Handle HTML entities
            if (c == '&' && i + 3 < html.length()) {
                String entity = "";
                int j = i + 1;
                while (j < html.length() && j < i + 10 && html.charAt(j) != ';') {
                    entity += html.charAt(j);
                    j++;
                }
                if (j < html.length() && html.charAt(j) == ';') {
                    // Common entities
                    if (entity == "nbsp") result += ' ';
                    else if (entity == "lt") result += '<';
                    else if (entity == "gt") result += '>';
                    else if (entity == "amp") result += '&';
                    else if (entity == "quot") result += '"';
                    else if (entity == "apos") result += '\'';
                    else if (entity == "ndash" || entity == "mdash") result += '-';
                    else if (entity == "hellip") result += "...";
                    else if (entity == "rsquo" || entity == "lsquo") result += '\'';
                    else if (entity == "rdquo" || entity == "ldquo") result += '"';
                    else result += ' '; // Unknown entity, add space
                    i = j;
                    continue;
                }
            }
            result += c;
        }
    }
    
    // Clean up multiple spaces and newlines
    String cleaned = "";
    bool lastWasSpace = false;
    for (int i = 0; i < result.length(); i++) {
        char c = result.charAt(i);
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            if (!lastWasSpace) {
                cleaned += ' ';
                lastWasSpace = true;
            }
        } else {
            cleaned += c;
            lastWasSpace = false;
        }
    }
    
    html = cleaned;
}

static void loadChapter(int index) {
    if (index < 0 || index >= s_state.spine.size()) return;
    
    String chName = s_state.spine[index];
    logger_log("EPUB: Loading chapter %d: %s", index, chName.c_str());
    oled_showStatus("Loading...");
    
    String content;
    bool success = getZipEntryData(s_state.currentBookPath, chName, content);
    
    if (success && content.length() > 0) {
         logger_log("EPUB: Loaded %d bytes", content.length());
         s_state.currentChapterText = content;
         
         // Strip HTML tags
         stripTags(s_state.currentChapterText);
         logger_log("EPUB: After strip: %d bytes", s_state.currentChapterText.length());
    } else {
         logger_log("EPUB: Failed to load chapter");
         s_state.currentChapterText = "Error loading chapter: " + chName;
    }

    // Clean HTML tags (basic strip)
    // TODO: Implement proper HTML tag stripping
    
    s_state.pageIndex = 0;
    // Calculate pages (must match updateEpaper constants)
    const int CHARS_PER_LINE = 19;
    const int LINES_PER_PAGE = 24;
    const int CHARS_PER_PAGE = CHARS_PER_LINE * LINES_PER_PAGE; // 456
    s_state.totalPages = (s_state.currentChapterText.length() + CHARS_PER_PAGE - 1) / CHARS_PER_PAGE;
    if (s_state.totalPages < 1) s_state.totalPages = 1;
    
    updateEpaper();
}


// --- Server Routes ---

namespace EpubApp {

void registerRoutes(void* serverPtr) {
    WebServer* server = (WebServer*)serverPtr;
    
    // List Epubs
    server->on("/api/epub/list", HTTP_GET, [server](){
        DynamicJsonDocument doc(4096);
        JsonArray arr = doc.to<JsonArray>();
        
        if (!LittleFS.exists("/epubs")) LittleFS.mkdir("/epubs");
        File dir = LittleFS.open("/epubs");
        File file = dir.openNextFile();
        while(file){
            String name = file.name();
            if (name.endsWith(".epub")) {
                JsonObject obj = arr.createNestedObject();
                obj["name"] = name;
                obj["size"] = file.size();
            }
            file = dir.openNextFile();
        }
        String out; serializeJson(doc, out);
        server->send(200, "application/json", out);
    });
    
    // Upload Epub
    server->on("/api/epub/upload", HTTP_POST, [server](){
        server->send(200, "text/plain", "OK");
    }, [server](){
        HTTPUpload& upload = server->upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if(!filename.startsWith("/")) filename = "/" + filename;
            if(!filename.endsWith(".epub")) {
                 // reject?
            }
            String path = "/epubs/" + filename;
            // Fix double slashes?
            path.replace("//", "/");
            
            // Ensure dir
            if (!LittleFS.exists("/epubs")) LittleFS.mkdir("/epubs");
            
            File f = LittleFS.open(path, "w");
            if (!f) {
                logger_log("Failed to open %s", path.c_str());
            }
            f.close(); // just create
            logger_log("Upload Start: %s", path.c_str());
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            String filename = upload.filename;
            if(!filename.startsWith("/")) filename = "/" + filename;
            String path = "/epubs/" + filename;
             path.replace("//", "/");
             
            File f = LittleFS.open(path, "a");
            if (f) {
                f.write(upload.buf, upload.currentSize);
                f.close();
            }
        } else if (upload.status == UPLOAD_FILE_END) {
             logger_log("Upload End: %d bytes", upload.totalSize);
        }
    });

    // Rename Epub
    server->on("/api/epub/rename", HTTP_POST, [server](){
         if (!server->hasArg("oldName") || !server->hasArg("newName")) {
            server->send(400, "application/json", "{\"error\":\"missing args\"}");
            return;
        }
        String oldName = server->arg("oldName");
        String newName = server->arg("newName");
        
        // Basic sanitization
        if (oldName.indexOf("..") >= 0 || newName.indexOf("..") >= 0 || !newName.endsWith(".epub")) {
             server->send(400, "application/json", "{\"error\":\"invalid name\"}");
             return;
        }
        
        if (!oldName.startsWith("/")) oldName = "/epubs/" + oldName;
        if (!newName.startsWith("/")) newName = "/epubs/" + newName;
        
        oldName.replace("//", "/");
        newName.replace("//", "/");
        
        if (LittleFS.rename(oldName, newName)) {
            server->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server->send(500, "application/json", "{\"error\":\"rename failed\"}");
        }
    });

    // Delete Epub
    server->on("/api/epub/delete", HTTP_POST, [server](){
        if (!server->hasArg("name")) {
            server->send(400, "application/json", "{\"error\":\"missing name\"}");
            return;
        }
        String name = server->arg("name");
        // security check
        if (name.indexOf("..") >= 0) {
             server->send(400, "application/json", "{\"error\":\"invalid path\"}");
             return;
        }
        
        String path = "/epubs/" + name;
        if (!name.startsWith("/")) path = "/epubs/" + name; // ensure prefix if missing
        
        path.replace("//", "/");
        
        if (LittleFS.remove(path)) {
            server->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server->send(500, "application/json", "{\"error\":\"delete failed\"}");
        }
    });
}

}
