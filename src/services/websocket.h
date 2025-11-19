#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <map>
#include <set>
#include <string>
#include "../config.h"
#include "../types.h"

// Note: WS_DELTA_MIN_MS and WS_CLEANUP_MS are defined in config.h
// Note: PathValue and ClientSubscription structs are defined in types.h

// ====== EXTERN DECLARATIONS ======
// WebSocket server instance
extern AsyncWebSocket ws;

// Client subscriptions
extern std::map<uint32_t, ClientSubscription> clientSubscriptions;

// Client authentication tokens
extern std::map<uint32_t, String> clientTokens;

// Data storage
extern std::map<String, PathValue> dataStore;
extern std::map<String, PathValue> lastSentValues;

// Vessel identification
extern String vesselUUID;

// ====== FUNCTION DECLARATIONS ======

/**
 * @brief Broadcast delta messages to all subscribed WebSocket clients
 * Iterates through changed data in dataStore and sends to clients
 * that have subscribed to those paths
 */
void broadcastDeltas();

/**
 * @brief Process incoming WebSocket messages
 * Handles:
 * - Subscribe/unsubscribe requests
 * - Incoming delta updates from clients
 * - Login/authentication messages
 *
 * @param client The WebSocket client
 * @param data The incoming message data
 * @param len The length of the data
 */
void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);

/**
 * @brief Main WebSocket event handler
 * Processes connection/disconnection and delegates message handling
 *
 * @param server The AsyncWebSocket server instance
 * @param client The WebSocket client
 * @param type The event type (WS_EVT_CONNECT, WS_EVT_DISCONNECT, WS_EVT_DATA, WS_EVT_ERROR)
 * @param arg Argument pointer (event-specific)
 * @param data Data buffer for WS_EVT_DATA
 * @param len Length of data
 */
void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                     AwsEventType type, void* arg, uint8_t* data, size_t len);

#endif // WEBSOCKET_H
