#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <deque>

// Initialize the logger (optional if just using static structures)
void logger_init();

// Log a formatted message (printf style) to Serial and internal buffer
// Format: "[LEVEL] message"
void logger_log(const char* fmt, ...);

// Retrieve the current log buffer
const std::deque<String>& logger_getLogs();

// Clear logs
void logger_clear();

#endif
