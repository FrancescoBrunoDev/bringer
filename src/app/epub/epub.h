#pragma once

#include "../ui/common/types.h"
#include <vector>
#include <Arduino.h>

// The App instance for the App Registry
extern const App APP_EPUB;

// Public API for server routes
namespace EpubApp {
    void registerRoutes(void* serverPtr); // Pass WebServer* as void* to avoid include cycles
}
