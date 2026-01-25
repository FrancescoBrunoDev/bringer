#include "logger.h"
#include <Arduino.h>
#include <deque>
#include <stdarg.h>

static std::deque<String> logBuffer;
static const size_t MAX_LOGS = 50;

void logger_init() {
    // Reserve if needed, though deque handles it.
    // Serial should be initialized in main setup.
}

void logger_log(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Add timestamp or just raw? 
    // For now user requested simple structure, we'll stick to the message.
    String msg = String(buf);
    
    // Print to Serial
    Serial.println(msg);

    // Add to buffer
    // Limit string length to avoid extreme memory usage?
    if (msg.length() > 200) msg = msg.substring(0, 200) + "...";

    logBuffer.push_back(msg);
    if (logBuffer.size() > MAX_LOGS) {
        logBuffer.pop_front();
    }
}

const std::deque<String>& logger_getLogs() {
    return logBuffer;
}

void logger_clear() {
    logBuffer.clear();
}
