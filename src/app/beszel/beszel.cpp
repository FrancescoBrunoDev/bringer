#include "beszel.h"
#include "secrets.h"
#include "utils/network_utils.h"
#include <ArduinoJson.h>
#include "utils/logger/logger.h"

BeszelService& BeszelService::getInstance() {
    static BeszelService instance;
    return instance;
}

bool BeszelService::begin(const String& baseUrl) {
    _baseUrl = baseUrl;
    if (!_baseUrl.endsWith("/")) {
        _baseUrl += "/";
    }
    _isInitialized = true;
    return true;
}

bool BeszelService::fetchSystems() {
    if (!_isInitialized) return false;

    String url = _baseUrl + "api/collections/systems/records";
    const char* token = (strlen(BESZEL_TOKEN) > 0) ? BESZEL_TOKEN : nullptr;

    String payload = net_httpGet(url, token);
    if (payload.length() == 0) return false;

    DynamicJsonDocument doc(8192); // Adjusted for multiple systems
    DeserializationError error = deserializeJson(doc, payload); // Revert to generic deserializeJson call if possible without templates issues, or keep simple
    
    if (error) {
        logger_log("Beszel: deserialize error: %s", error.c_str());
        return false;
    }

    JsonArray items = doc["items"];
    _systems.clear();
    
    for (JsonObject item : items) {
        BeszelSystem sys;
        sys.id = item["id"].as<String>();
        sys.name = item["name"].as<String>();
        sys.host = item["host"].as<String>();
        sys.status = item["status"].as<String>();
        
        // Stats are often in a JSON field named 'info'
        // In Beszel, 'info' might be an object or a string depending on version
        if (item.containsKey("info")) {
            if (item["info"].is<JsonObject>()) {
                JsonObject info = item["info"];
                sys.cpu = info["cpu"] | 0.0f;
                sys.mem = info["mp"] | 0.0f;
                sys.disk = info["dp"] | 0.0f;
                sys.net = info["bb"] | 0.0f;
            } else if (item["info"].is<const char*>()) {
                // Try parsing string if it's a string
                String infoStr = item["info"].as<String>();
                StaticJsonDocument<512> infoDoc;
                if (deserializeJson(infoDoc, infoStr) == DeserializationError::Ok) {
                    sys.cpu = infoDoc["cpu"] | 0.0f;
                    sys.mem = infoDoc["mp"] | 0.0f;
                    sys.disk = infoDoc["dp"] | 0.0f;
                    sys.net = infoDoc["bb"] | 0.0f;
                }
            }
        }
        
        _systems.push_back(sys);
    }
    
    return true;
}
