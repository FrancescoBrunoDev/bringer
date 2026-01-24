#include "apps.h"
#include "../../rss/rss.h"
#include "../common/components.h"
#include "../ui_internal.h"
#include "drivers/oled/oled.h"
#include "drivers/epaper/display.h"
#include "drivers/epaper/layout.h"
#include <GxEPD2_3C.h>

// Navigation state
static uint8_t s_index = 0;
static uint8_t s_prevIndex = 0;
static unsigned long s_lastFetch = 0;
static const unsigned long FETCH_INTERVAL = 300000; // 5 minutes
static RSSFeed s_feed;

// Viewing mode state
static bool s_viewingArticle = false;
static int16_t s_scrollOffset = 0;
static int16_t s_componentIndex = 0; // Current starting component index for pagination
static std::vector<EpdComponent> s_currentArticleComponents;
static String s_currentArticleTitle;

static void fetch_data() {
    if (oled_isAvailable()) oled_showToast("Fetching NYT...", 1000);
    if (RSSService::getInstance().fetchNYT(s_feed, 30)) {
        if (oled_isAvailable()) oled_showToast("News Updated", 800);
    } else {
        if (oled_isAvailable()) oled_showToast("Fetch Failed", 1500);
    }
    s_lastFetch = millis();
}

static void render_news_item(uint8_t index, int16_t x, int16_t y) {
    if (index < s_feed.items.size()) {
        const auto& item = s_feed.items[index];
        
        // Truncate title if too long
        String displayTitle = item.title;
        if (displayTitle.length() > 40) {
            displayTitle = displayTitle.substring(0, 37) + "...";
        }
        
        char buf[48];
        snprintf(buf, sizeof(buf), "%s", displayTitle.c_str());
        oled_drawBigText(buf, x, y, false);
    } else {
        oled_drawBigText("No News", x, y, false);
    }
}

static void view_render(int16_t x_offset, int16_t y_offset) {
    if (s_viewingArticle) {
        // Show Page X/Y instead of "Reading..."
        int maxComponents = 24; // Must match render_article_with_offset
        int pageNum = (s_componentIndex / maxComponents) + 1;
        int totalPages = (s_currentArticleComponents.size() + maxComponents - 1) / maxComponents;
        if (totalPages < 1) totalPages = 1;
        
        char buf[32];
        snprintf(buf, sizeof(buf), "Page %d/%d", pageNum, totalPages);
        oled_drawBigText(buf, x_offset, y_offset, false);
        return;
    }
    
    if (s_feed.items.size() == 0) {
        oled_drawBigText("No Data", x_offset, y_offset, false);
        return;
    }

    if (abs(y_offset) < 1) {
        render_news_item(s_index, x_offset, 0);
    } else {
        render_news_item(s_index, x_offset, y_offset);
        render_news_item(s_prevIndex, x_offset, y_offset > 0 ? y_offset - 64 : y_offset + 64);
    }
}

// Helper to add wrapped text rows
static void add_wrapped_text(const String& content, const String& prefix = "") {
    String fullText = prefix + content;
    int pos = 0;
    while (pos < fullText.length()) {
        // Increased to 18 chars thanks to reduced margins (12px gain ~ 2 char)
        int chunkSize = min(18, (int)(fullText.length() - pos));
        if (pos + chunkSize < fullText.length()) {
            int lastSpace = fullText.lastIndexOf(' ', pos + chunkSize);
            if (lastSpace > pos) chunkSize = lastSpace - pos;
        }
        String chunk = fullText.substring(pos, pos + chunkSize);
        chunk.trim();
        if (chunk.length() > 0) {
            // Use ROW with only text1 (Left aligned)
            s_currentArticleComponents.push_back({EPD_COMP_ROW, chunk, "", 0, GxEPD_BLACK});
        }
        pos += chunkSize;
        if (pos < fullText.length() && fullText[pos] == ' ') pos++;
    }
}

static void prepare_article_components(const RSSItem& item) {
    s_currentArticleComponents.clear();
    s_currentArticleTitle = item.title;
    
    // 1. Title as header
    String cleanTitle = item.title;
    int pos = 0;
    // bool first = true; // No longer needed, all title lines are headers
    while (pos < cleanTitle.length()) {
        int chunkSize = min(18, (int)(cleanTitle.length() - pos)); // 18 chars limit
        if (pos + chunkSize < cleanTitle.length()) {
            int lastSpace = cleanTitle.lastIndexOf(' ', pos + chunkSize);
            if (lastSpace > pos) chunkSize = lastSpace - pos;
        }
        String chunk = cleanTitle.substring(pos, pos + chunkSize);
        chunk.trim();
        if (chunk.length() > 0) {
            // All title chunks are now HEADERS to be bold
            s_currentArticleComponents.push_back({EPD_COMP_HEADER, chunk, "", 0, GxEPD_BLACK});
        }
        pos += chunkSize;
        if (pos < cleanTitle.length() && cleanTitle[pos] == ' ') pos++;
    }
    
    // 2. Author - removed separator for compactness
    if (!item.author.isEmpty()) {
        // s_currentArticleComponents.push_back({EPD_COMP_SEPARATOR, "", "", 0, 0});
        add_wrapped_text(item.author); 
    }
    
    // 3. Description
    if (!item.description.isEmpty()) {
        String cleanDesc = item.description;
        
        // Remove HTML tags
        int tagStart;
        while ((tagStart = cleanDesc.indexOf('<')) != -1) {
            int tagEnd = cleanDesc.indexOf('>', tagStart);
            if (tagEnd == -1) break;
            cleanDesc.remove(tagStart, tagEnd - tagStart + 1);
        }
        
        // Decode entities
        cleanDesc.replace("&amp;", "&");
        cleanDesc.replace("&lt;", "<");
        cleanDesc.replace("&gt;", ">");
        cleanDesc.replace("&quot;", "\"");
        cleanDesc.replace("&#x2F;", "/");
        cleanDesc.replace("&#x27;", "'");
        cleanDesc.replace("&#8217;", "'");
        cleanDesc.replace("&#8220;", "\"");
        cleanDesc.replace("&#8221;", "\"");
        
        cleanDesc.trim();
        
        if (cleanDesc.length() > 0) {
            s_currentArticleComponents.push_back({EPD_COMP_SEPARATOR, "", "", 0, 0});
            add_wrapped_text(cleanDesc);
        }
    }
    
    // 4. Date - removed separator
    if (!item.pubDate.isEmpty()) {
        // Compact visual separator
        s_currentArticleComponents.push_back({EPD_COMP_ROW, "---", "", 0, GxEPD_BLACK});
        add_wrapped_text(item.pubDate);
    }
}



static void render_article_with_offset(int16_t startIdx) {
    if (epd_isBusy()) {
        if (oled_isAvailable()) oled_showToast("EPD busy", 1000);
        return;
    }

    EpdPage page;
    page.title = ""; // No header, maximum space
    
    // Calculate how many components fit
    // Back to 24 lines since we removed the header
    int maxComponents = 24; 
    
    int total = s_currentArticleComponents.size();
    int endIdx = min(total, (int)startIdx + maxComponents);
    
    for (int i = startIdx; i < endIdx; i++) {
        page.components.push_back(s_currentArticleComponents[i]);
    }
    
    if (oled_isAvailable()) {
        char buf[32];
        int pageNum = (startIdx / maxComponents) + 1;
        int totalPages = (total + maxComponents - 1) / maxComponents;
        snprintf(buf, sizeof(buf), "Page %d/%d", pageNum, totalPages);
        oled_showToast(buf, 800);
    }
    
    epd_displayPage(page);
}

static void view_next(void) {
    if (s_viewingArticle) {
        // Prevent state drift: don't change page index if display is busy
        if (epd_isBusy()) {
            if (oled_isAvailable()) oled_showToast("Wait...", 500);
            return;
        }

        // Page Scroll Down
        int maxComponents = 24;
        if (s_componentIndex + maxComponents < s_currentArticleComponents.size()) {
            s_componentIndex += maxComponents;
            render_article_with_offset(s_componentIndex);
        } else {
             if (oled_isAvailable()) oled_showToast("End of article", 800);
        }
        return;
    }
    
    if (s_feed.items.size() <= 1) return;
    s_prevIndex = s_index;
    s_index = (s_index + 1) % (uint8_t)s_feed.items.size();
    ui_triggerVerticalAnimation(true);
}

static void view_prev(void) {
    if (s_viewingArticle) {
        // Prevent state drift: don't change page index if display is busy
        if (epd_isBusy()) {
            if (oled_isAvailable()) oled_showToast("Wait...", 500);
            return;
        }

        // Page Scroll Up
        int maxComponents = 24;
        if (s_componentIndex > 0) {
            s_componentIndex = max(0, s_componentIndex - maxComponents);
            render_article_with_offset(s_componentIndex);
        } else {
             if (oled_isAvailable()) oled_showToast("Start of article", 800);
        }
        return;
    }
    
    if (s_feed.items.size() <= 1) return;
    s_prevIndex = s_index;
    s_index = (s_index + (uint8_t)s_feed.items.size() - 1) % (uint8_t)s_feed.items.size();
    ui_triggerVerticalAnimation(false);
}

static void view_select(void) {
    if (s_viewingArticle) {
        render_article_with_offset(s_componentIndex); // Refresh
        return;
    }
    
    if (s_index < s_feed.items.size()) {
        s_viewingArticle = true;
        s_componentIndex = 0; // Reset to top
        prepare_article_components(s_feed.items[s_index]);
        render_article_with_offset(0);
        if (oled_isAvailable()) oled_showToast("Reading mode", 1000);
    } else {
        fetch_data();
    }
}

static void view_back(void) {
    if (s_viewingArticle) {
        s_viewingArticle = false;
        s_componentIndex = 0;
        s_currentArticleComponents.clear();
        if (oled_isAvailable()) oled_showToast("Back to list", 800);
        ui_redraw();
        return;
    }
    
    ui_setView(NULL);
}

static float view_get_progress(void) {
    if (s_viewingArticle) {
        if (s_currentArticleComponents.size() == 0) return 0.0f;
        return (float)s_componentIndex / (float)s_currentArticleComponents.size();
    }
    
    if (s_feed.items.size() == 0) return 0.0f;
    return (float)(s_index + 1) / (float)s_feed.items.size();
}

static void view_poll(void) {
}

static const View VIEW_NYT = {
    "NY Times",
    view_render,
    view_next,
    view_prev,
    view_select,
    view_back,
    view_poll,
    view_get_progress
};

static void app_renderPreview(int16_t x_offset, int16_t y_offset) {
    size_t count = s_feed.items.size();
    char buf[24];
    snprintf(buf, sizeof(buf), "%zu articles", count);
    comp_title_and_text("NY TIMES", count > 0 ? buf : "No data", x_offset, y_offset, false);
}

static void app_select(void) {
    s_index = 0;
    s_viewingArticle = false;
    s_componentIndex = 0;
    ui_setView(&VIEW_NYT);
    if (s_feed.items.size() == 0) {
        fetch_data();
    }
}

static void app_poll(void) {
}

const App APP_NYT = {
    "NY Times",
    app_renderPreview,
    app_select,
    app_poll
};
