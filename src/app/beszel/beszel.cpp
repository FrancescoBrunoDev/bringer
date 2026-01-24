#include "beszel.h"
#include "secrets.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

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

    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate validation for convenience

    HTTPClient http;
    String url = _baseUrl + "api/collections/systems/records";
    
    http.begin(client, url);
    
    if (strlen(BESZEL_TOKEN) > 0) {
        http.addHeader("Authorization", String("Bearer ") + BESZEL_TOKEN);
    }
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(8192); // Adjusted for multiple systems
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            http.end();
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
        
        http.end();
        return true;
    } else {
        Serial.print("HTTP GET failed, error: ");
        Serial.println(http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }
}
