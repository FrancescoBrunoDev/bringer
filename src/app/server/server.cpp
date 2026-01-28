/*
  server.cpp
  Generic HTTP server that delegates route registration to the AppRegistry.
*/

#include "server.h"
#include "config.h"
#include "app/registry.h"
#include "utils/logger/logger.h"
#include <WebServer.h>

static WebServer server(WEB_SERVER_PORT);

void server_init() {
  // Register generic not found handler
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  // Delegate route registration to all registered apps
  AppRegistry::registerAllRoutes(&server);

  server.begin();
  logger_log("HTTP server started");
}

void server_handleClient() {
  server.handleClient();
}
