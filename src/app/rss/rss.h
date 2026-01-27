#pragma once

#include <Arduino.h>
#include <vector>

struct RSSItem {
    String title;
    String link;
    String description;
    String pubDate;
    String author;
};

struct RSSFeed {
    String title;
    String link;
    String description;
    std::vector<RSSItem> items;
};

class RSSService {
public:
    static RSSService& getInstance();
    
    // Fetch and parse an RSS feed from the given URL
    bool fetchFeed(const String& url, RSSFeed& feed, size_t maxItems = 20);
    
    // New York Times feed
    bool fetchNYT(RSSFeed& feed, size_t maxItems = 20);
    
private:
    RSSService() {}
    
    // Parse RSS XML content
    bool parseRSS(const String& xml, RSSFeed& feed, size_t maxItems);
    
    // Helper to extract text between tags
    String extractTag(const String& xml, const String& tag, size_t& pos);
};
