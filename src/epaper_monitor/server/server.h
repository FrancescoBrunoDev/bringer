#pragma once

/**
 * server.h
 *
 * Small wrapper for the HTTP server used by the ESP32 e-paper example.
 *
 * Responsibilities:
 *  - Register HTTP endpoints (/, /status, /text, /image, /img, /clear)
 *  - Start the server
 *  - Provide a small loop helper to process incoming requests
 *
 * Usage:
 *  - Call `server_init()` from `setup()` after network is configured.
 *  - Call `server_handleClient()` from `loop()` to process clients.
 */

#include <Arduino.h>

// Initialize and start the HTTP server (register all endpoints).
void server_init();

// Must be called frequently from the main `loop()` to handle incoming requests.
void server_handleClient();