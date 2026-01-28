#include "registry.h"
#include <Arduino.h>

static std::vector<const App*> g_apps;

namespace AppRegistry {
    void registerApp(const App* app) {
        if (app) {
            g_apps.push_back(app);
            Serial.printf("AppRegistry: Registered app '%s'\n", app->name);
        }
    }

    void setupAll() {
        for (const auto* app : g_apps) {
            if (app->setup) {
                Serial.printf("AppRegistry: Setup '%s'\n", app->name);
                app->setup();
            }
        }
    }

    void registerAllRoutes(void* serverPtr) {
        for (const auto* app : g_apps) {
            if (app->registerRoutes) {
                Serial.printf("AppRegistry: Register routes for '%s'\n", app->name);
                app->registerRoutes(serverPtr);
            }
        }
    }

    void pollAll() {
        for (const auto* app : g_apps) {
            if (app->poll) {
                app->poll();
            }
        }
    }

    const std::vector<const App*>& getApps() {
        return g_apps;
    }
}
