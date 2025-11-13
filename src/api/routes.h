#ifndef API_ROUTES_H
#define API_ROUTES_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

/**
 * Set up all HTTP routes for the SignalK server
 * This includes REST API endpoints, WebSocket, and web UI routes
 *
 * @param server Reference to the AsyncWebServer instance
 */
void setupRoutes(AsyncWebServer& server);

#endif  // API_ROUTES_H
