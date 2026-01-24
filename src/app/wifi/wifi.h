#pragma once

/*
 * app/wifi/wifi.h
 *
 * Small WiFi helper API for the ESP32 e-paper example.
 *
 * Behavior:
 *  - `connectWiFi()` attempts to connect in STA mode using credentials from `secrets.h`.
 *    If it fails within a timeout it falls back to starting a soft-AP (`startAP()`).
 *
 *  - `startAP()` starts a soft-AP using `AP_SSID`/`AP_PASSWORD` from `secrets.h`.
 *
 *  - `wifi_getIP()` returns the active IP address (STA if connected, otherwise AP IP).
 *  - `wifi_isConnected()` returns true when in STA mode and connected to an AP.
 *
 * Notes:
 *  - `secrets.h` should provide `WIFI_SSID`, `WIFI_PASSWORD`, `AP_SSID`, `AP_PASSWORD`.
 *  - Uses `WIFI_CONNECT_TIMEOUT_MS` from `config.h` if present; otherwise you can
 *    implement a custom timeout in the corresponding .cpp.
 */

#include <Arduino.h>
#include <WiFi.h>

#ifdef __cplusplus
extern "C" {
#endif

// Try to connect as a station, fall back to AP if connection fails.
void connectWiFi();

// Start a soft-AP (useful when STA credentials are not available)
void startAP();

// Return the active IP address (STA if connected, otherwise softAP IP)
IPAddress wifi_getIP();

// Return true when connected as STA to an access point
bool wifi_isConnected();

// Return the current SSID (STA or AP)
String wifi_getSSID();

#ifdef __cplusplus
} // extern "C"
#endif