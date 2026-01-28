#pragma once

#include <Arduino.h>

/**
 * @brief Performs an HTTP GET request to the specified URL.
 * 
 * Handles WiFi connection check, HTTPS (insecure), timeouts, and optional Authorization header.
 * Logs errors via logger_log.
 * 
 * @param url The target URL.
 * @param authToken Optional Bearer token.
 * @param timeoutMs Request timeout in milliseconds (default 10000).
 * @return String The response payload, or empty string on failure.
 */
String net_httpGet(const String& url, const char* authToken = nullptr, uint32_t timeoutMs = 10000);

/**
 * @brief Performs an HTTP POST request to the specified URL.
 * 
 * @param url The target URL.
 * @param jsonPayload The JSON string to send in body.
 * @param authToken Optional Bearer token.
 * @param timeoutMs Request timeout in milliseconds (default 10000).
 * @return String The response payload, or empty string on failure.
 */
String net_httpPost(const String& url, const String& jsonPayload, const char* authToken = nullptr, uint32_t timeoutMs = 10000);
