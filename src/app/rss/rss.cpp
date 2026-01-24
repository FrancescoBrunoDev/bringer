#include "rss.h"
#include <HTTPClient.h>
#include <WiFi.h>

RSSService& RSSService::getInstance() {
    static RSSService instance;
    return instance;
}

bool RSSService::fetchNYT(RSSFeed& feed, size_t maxItems) {
    // NYT Home Page RSS feed URL
    return fetchFeed("https://rss.nytimes.com/services/xml/rss/nyt/HomePage.xml", feed, maxItems);
}

bool RSSService::fetchFeed(const String& url, RSSFeed& feed, size_t maxItems) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected");
        return false;
    }
    
    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000); // 10 second timeout
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed, error: %d\n", httpCode);
        http.end();
        return false;
    }
    
    String payload = http.getString();
    http.end();
    
    if (payload.length() == 0) {
        Serial.println("Empty response");
        return false;
    }
    
    return parseRSS(payload, feed, maxItems);
}

String RSSService::extractTag(const String& xml, const String& tag, size_t& pos) {
    String openTag = "<" + tag + ">";
    String closeTag = "</" + tag + ">";
    
    int start = xml.indexOf(openTag, pos);
    if (start == -1) return "";
    
    start += openTag.length();
    int end = xml.indexOf(closeTag, start);
    if (end == -1) return "";
    
    pos = end + closeTag.length();
    
    String content = xml.substring(start, end);
    content.trim();
    return decodeHTML(content);
}

String RSSService::decodeHTML(const String& str) {
    String result = str;
    
    // Common HTML entities
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&apos;", "'");
    result.replace("&#39;", "'");
    result.replace("&nbsp;", " ");
    
    // Remove CDATA tags if present
    result.replace("<![CDATA[", "");
    result.replace("]]>", "");
    
    return result;
}

bool RSSService::parseRSS(const String& xml, RSSFeed& feed, size_t maxItems) {
    feed.items.clear();
    
    // Extract channel info
    size_t pos = 0;
    
    // Find channel section
    int channelStart = xml.indexOf("<channel>");
    if (channelStart == -1) {
        Serial.println("No channel found in RSS");
        return false;
    }
    
    pos = channelStart;
    
    // Extract feed title (before first item)
    size_t titlePos = pos;
    feed.title = extractTag(xml, "title", titlePos);
    
    size_t linkPos = pos;
    feed.link = extractTag(xml, "link", linkPos);
    
    size_t descPos = pos;
    feed.description = extractTag(xml, "description", descPos);
    
    // Parse items
    pos = channelStart;
    size_t itemCount = 0;
    
    while (itemCount < maxItems) {
        int itemStart = xml.indexOf("<item>", pos);
        if (itemStart == -1) break;
        
        int itemEnd = xml.indexOf("</item>", itemStart);
        if (itemEnd == -1) break;
        
        String itemXml = xml.substring(itemStart, itemEnd + 7);
        
        RSSItem item;
        size_t itemPos = 0;
        
        item.title = extractTag(itemXml, "title", itemPos);
        
        itemPos = 0;
        item.link = extractTag(itemXml, "link", itemPos);
        
        itemPos = 0;
        item.description = extractTag(itemXml, "description", itemPos);
        
        itemPos = 0;
        item.pubDate = extractTag(itemXml, "pubDate", itemPos);
        
        // Try different author tags
        itemPos = 0;
        item.author = extractTag(itemXml, "author", itemPos);
        if (item.author.isEmpty()) {
            itemPos = 0;
            item.author = extractTag(itemXml, "dc:creator", itemPos);
        }
        
        // Only add if we have at least a title
        if (!item.title.isEmpty()) {
            feed.items.push_back(item);
            itemCount++;
        }
        
        pos = itemEnd + 7;
    }
    
    Serial.printf("Parsed %d items from RSS feed\n", feed.items.size());
    return feed.items.size() > 0;
}
