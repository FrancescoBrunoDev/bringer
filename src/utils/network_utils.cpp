#include "network_utils.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "app/wifi/wifi.h"
#include "utils/logger/logger.h"

String net_httpGet(const String& url, const char* authToken, uint32_t timeoutMs) {
    if (!wifi_isConnected()) {
        logger_log("Net: WiFi not connected");
        return "";
    }

    WiFiClient* client = nullptr;
    WiFiClientSecure secureClient;
    WiFiClient insecureClient;

    if (url.startsWith("https")) {
        secureClient.setInsecure(); // Skip certificate validation
        client = &secureClient;
    } else {
        client = &insecureClient;
    }

    HTTPClient http;
    http.begin(*client, url);
    http.setTimeout(timeoutMs);

    if (authToken && strlen(authToken) > 0) {
        http.addHeader("Authorization", String("Bearer ") + authToken);
    }

    int httpCode = http.GET();
    String payload = "";

    if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
        if (payload.length() == 0) {
            logger_log("Net: Empty response from %s", url.c_str());
        }
    } else {
        String err = http.errorToString(httpCode);
        logger_log("Net: GET failed %s (Code: %d)", err.c_str(), httpCode);
    }

    http.end();
    return payload;
}

String net_httpPost(const String& url, const String& jsonPayload, const char* authToken, uint32_t timeoutMs) {
    if (!wifi_isConnected()) {
        logger_log("Net: WiFi not connected");
        return "";
    }

    WiFiClient* client = nullptr;
    WiFiClientSecure secureClient;
    WiFiClient insecureClient;

    if (url.startsWith("https")) {
        secureClient.setInsecure();
        client = &secureClient;
    } else {
        client = &insecureClient;
    }

    HTTPClient http;
    http.begin(*client, url);
    http.setTimeout(timeoutMs);
    http.addHeader("Content-Type", "application/json");

    if (authToken && strlen(authToken) > 0) {
        http.addHeader("Authorization", String("Bearer ") + authToken);
    }

    int httpCode = http.POST(jsonPayload);
    String payload = "";

    if (httpCode > 0) {
        payload = http.getString();
        if (httpCode >= 200 && httpCode < 300) {
             // Success
        } else {
            logger_log("Net: POST failed code %d: %s", httpCode, payload.c_str());
        }
    } else {
        String err = http.errorToString(httpCode);
        logger_log("Net: POST connection failed %s", err.c_str());
    }

    http.end();
    return httpCode >= 200 && httpCode < 300 ? payload : "";
}
