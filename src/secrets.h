// secrets.h - WiFi credentials for the ESP32-C6 ePaper API project
// This file no longer stores real credentials. Use a local `.env` file
// (ignored by git) to provide secrets during the build. A pre-build
// script generates `src/generated_secrets.h` from `.env` when present.

#ifndef SECRETS_H
#define SECRETS_H

// If a generated header exists include it. We use a guarded include so
// builds do not fail when `.env` / generated file is absent.
#if defined(__has_include)
#  if __has_include("generated_secrets.h")
#    include "generated_secrets.h"
#  endif
#endif

// Fallback values used when no generated header is available. These are
// safe placeholders; replace them by creating a `.env` file at the
// repository root (see `.env.example`).
#ifndef WIFI_SSID
#define WIFI_SSID "CHANGE_ME"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "CHANGE_ME"
#endif

#ifndef AP_SSID
#define AP_SSID "EPaper-AP"
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "epaper123"
#endif

// Optional AP IP (leave undefined to use DHCP)
#ifndef AP_IP_ADDRESS
// #define AP_IP_ADDRESS "192.168.4.1"
#endif

#ifndef BESZEL_TOKEN
#define BESZEL_TOKEN ""
#endif

#ifndef HA_URL
#define HA_URL "http://raspberrypi:8123/"
#endif

#ifndef HA_TOKEN
#define HA_TOKEN ""
#endif

#endif // SECRETS_H
