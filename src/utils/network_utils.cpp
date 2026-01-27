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
