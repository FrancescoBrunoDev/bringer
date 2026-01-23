/*
 * wifi.cpp
 *
 * Lightweight WiFi helper for the ESP32 e-paper API example.
 *
 * Responsibilities:
 *  - Connect in STA (client) mode using credentials from `secrets.h`.
 *  - Fall back to starting a soft-AP if STA connection fails.
 *  - Provide small helpers for retrieving the active IP and connection status.
 */

#include "wifi.h"
#include "config.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>

void connectWiFi() {
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect, starting AP...");
    startAP();
  }
}

void startAP() {
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (!ok) {
    Serial.println("Failed to start AP");
  } else {
    IPAddress apip = WiFi.softAPIP();
    Serial.print("AP started. Connect to SSID: ");
    Serial.print(AP_SSID);
    Serial.print("  IP: ");
    Serial.println(apip);
  }
}

IPAddress wifi_getIP() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP();
  }
  // If not connected as STA, return the softAP IP (may be 0.0.0.0 if AP not active)
  return WiFi.softAPIP();
}

bool wifi_isConnected() {
  return WiFi.status() == WL_CONNECTED;
}