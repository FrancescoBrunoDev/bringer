#include "ha_service.h"
#include "utils/network_utils.h"
#include "utils/logger/logger.h"
#include <ArduinoJson.h>

HAService& HAService::getInstance() {
    static HAService instance;
    return instance;
}

bool HAService::begin(const String& baseUrl, const String& token) {
    _baseUrl = baseUrl;
    _token = token;
    
    if (!_baseUrl.isEmpty() && !_baseUrl.endsWith("/")) {
        _baseUrl += "/";
    }
    
    _isInitialized = true;
    logger_log("HAService initialized");
    return true;
}

bool HAService::fetchList() {
    if (!_isInitialized) return false;
    
    String url = _baseUrl + "api/shopping_list";
    
    logger_log("Fetching HA list from: %s", url.c_str());
    
    String payload = net_httpGet(url, _token.c_str());
    if (payload.length() == 0) {
        logger_log("HA: Fetch failed (empty payload)");
        return false;
    }
    
    // Estimate size: 4KB should be plenty for a shopping list
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        logger_log("HA: deserialize error: %s", error.c_str());
        return false;
    }
    
    if (!doc.is<JsonArray>()) {
        logger_log("HA: JSON is not an array");
        return false;
    }
    
    _activeItems.clear();
    _completedItems.clear();
    
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject item : arr) {
        HAShoppingItem i;
        i.id = item["id"].as<String>();
        i.name = item["name"] | "Unknown";
        i.complete = item["complete"] | false;
        
        if (i.complete) {
            _completedItems.push_back(i);
        } else {
            _activeItems.push_back(i);
        }
    }
    
    logger_log("HA: Fetched %d active, %d completed", _activeItems.size(), _completedItems.size());
    return true;
}

bool HAService::setComplete(const String& itemId, bool complete) {
    if (!_isInitialized) return false;
    
    String url = _baseUrl + "api/shopping_list/item/" + itemId;
    logger_log("Setting item %s complete: %d", itemId.c_str(), complete);
    
    // JSON payload: {"complete": true/false}
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"complete\": %s}", complete ? "true" : "false");
    
    String response = net_httpPost(url, payload, _token.c_str());
    if (response.length() == 0) {
        logger_log("HA: Failed to update item");
        return false;
    }
    
    return true;
}
