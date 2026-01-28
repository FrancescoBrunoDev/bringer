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
#include "utils/zip_utils.h"
#include "utils/html_utils.h" 

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

    // Remove extension
    if (name.endsWith(".html")) name = name.substring(0, name.length() - 5);
    else if (name.endsWith(".xhtml")) name = name.substring(0, name.length() - 6);
    else if (name.endsWith(".htm")) name = name.substring(0, name.length() - 4);
    
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

    ZipReader reader;
    if (!reader.open(path)) {
        logger_log("EPUB: Failed to open %s", path.c_str());
        return false;
    }

    // Use callback-based scanning to save memory (don't load all filenames)
    int count = 0;
    uint32_t startHeap = ESP.getFreeHeap();
    logger_log("EPUB: Starting index. Heap: %d", startHeap);
    
    reader.processFileEntries([&](const String& name) -> bool {
        if (name.endsWith(".html") || name.endsWith(".xhtml") || name.endsWith(".htm")) {
             // Only log every 5 chapters to save serial time
             if (count % 5 == 0) {
                 logger_log("EPUB: Found ch %d: %s (Heap: %d)", count, name.c_str(), ESP.getFreeHeap());
             }
             s_state.spine.push_back(name);
             count++;
             
             // Safety limit for now to see if it's purely memory capacity
             if (count > 200) {
                 logger_log("EPUB: Limit reached (200 chapters). Stopping scan.");
                 return false;
             }
        }
        return true;
    });
    
    logger_log("EPUB: Index done. Chapters: %d. Heap: %d", s_state.spine.size(), ESP.getFreeHeap());
    
    reader.close();
    
    std::sort(s_state.spine.begin(), s_state.spine.end());
    logger_log("EPUB: Found %d chapters", s_state.spine.size());
    
    return !s_state.spine.empty();
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

         // Remove extension
         if (chName.endsWith(".html")) chName = chName.substring(0, chName.length() - 5);
         else if (chName.endsWith(".xhtml")) chName = chName.substring(0, chName.length() - 6);
         else if (chName.endsWith(".htm")) chName = chName.substring(0, chName.length() - 4);
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



static void loadChapter(int index) {
    if (index < 0 || index >= s_state.spine.size()) return;
    
    String chName = s_state.spine[index];
    logger_log("EPUB: Loading chapter %d: %s", index, chName.c_str());
    oled_showStatus("Loading...");
    
    uint8_t* rawBuf = NULL;
    size_t rawSize = 0;
    ZipReader reader;
    bool success = false;
    
    logger_log("EPUB: Opening ZIP...");
    if (reader.open(s_state.currentBookPath)) {
        logger_log("EPUB: ZIP opened, calling readBinary...");
        success = reader.readBinary(chName, &rawBuf, &rawSize);
        logger_log("EPUB: readBinary returned: %d", success);
        reader.close();
    } else {
        logger_log("EPUB: Failed to open ZIP");
    }
    
    if (success && rawBuf && rawSize > 0) {
         logger_log("EPUB: Loaded %d bytes, heap: %d", rawSize, ESP.getFreeHeap());
         
         logger_log("EPUB: Calling html_strip_tags_inplace...");
         // In-place strip
         // Treat rawBuf as char* (assuming it's text/html)
         // Ensure null termination just in case (readBinary does it)
         html_strip_tags_inplace((char*)rawBuf, rawSize);
         
         logger_log("EPUB: HTML stripped, assigning to String...");
         // Now rawBuf contains the stripped text
         // We can now assign it to String, or better yet, if we can keep it as char*?
         // s_state.currentChapterText is a String.
         // Assigning char* to String will copy it.
         // But at least we didn't have 3 copies in memory at once (Raw+String+Result).
         // We only had Raw -> (processed in place) -> Copy to String.
         // Still 2 copies effectively for a moment, but avoiding the big intermediate "HTML String" object.
         
         s_state.currentChapterText = (char*)rawBuf;
         
         logger_log("EPUB: String assigned");
         size_t plainSize = s_state.currentChapterText.length();
         logger_log("EPUB: After strip: %d bytes (was %d)", plainSize, rawSize);
         
         free(rawBuf); // Modified buffer is freed
    } else {
         logger_log("EPUB: Failed to load chapter");
         s_state.currentChapterText = "Error loading chapter: " + chName;
         if (rawBuf) free(rawBuf);
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
        static File uploadFile;

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
            
            // If exists, delete first to clear
            if (LittleFS.exists(path)) LittleFS.remove(path);

            uploadFile = LittleFS.open(path, "w");
            if (!uploadFile) {
                logger_log("Failed to open %s for writing", path.c_str());
            } else {
                logger_log("Upload Start: %s", path.c_str());
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (uploadFile) {
                uploadFile.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
             if (uploadFile) {
                 uploadFile.close();
                 logger_log("Upload End: %d bytes", upload.totalSize);
             } else {
                 logger_log("Upload End: File was not open");
             }
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
