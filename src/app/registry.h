#pragma once

#include "ui/common/types.h"
#include <vector>

class WebServer;

namespace AppRegistry {
    // Register an app to the system. Should be called before setupAll().
    void registerApp(const App* app);

    // Call setup() on all registered apps.
    void setupAll();

    // Register routes for all apps on the given server.
    // serverPtr is passed as void* to match the App function signature and avoid circular deps,
    // but internally it casts to WebServer*.
    void registerAllRoutes(void* serverPtr);

    // Call poll() on all registered apps.
    void pollAll();

    // Get the list of all registered apps (for UI carousel)
    const std::vector<const App*>& getApps();
}
