#include "websocket.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cmath>

// ====== EXTERN DECLARATIONS ======
// These are defined in main.cpp
// Note: Struct definitions are in types.h
extern GeofenceConfig geofence;
extern DepthAlarmConfig depthAlarm;
extern WindAlarmConfig windAlarm;
extern String serverName;

// Forward declarations of functions from main.cpp
extern String iso8601Now();
extern void setPathValue(const String& path, double value, const String& source, const String& units, const String& description);
extern void setPathValue(const String& path, const String& value, const String& source, const String& units, const String& description);
extern void setPathValueJson(const String& path, const String& jsonValue, const String& source, const String& units, const String& description);

// ====== WEBSOCKET DELTA BROADCAST ======
void broadcastDeltas() {
  // Removed throttling - broadcast immediately when data is available

  // Build delta message
  DynamicJsonDocument doc(4096);
  doc["context"] = "vessels." + vesselUUID;

  JsonArray updates = doc.createNestedArray("updates");
  JsonObject update = updates.createNestedObject();

  update["timestamp"] = iso8601Now();

  JsonObject source = update.createNestedObject("source");
  source["label"] = "ESP32-SignalK";
  source["type"] = "NMEA0183";

  JsonArray values = update.createNestedArray("values");

  bool hasChanges = false;

  for (auto& kv : dataStore) {
    if (!kv.second.changed) continue;

    JsonObject val = values.createNestedObject();
    val["path"] = kv.first;

    if (kv.second.isJson) {
      DynamicJsonDocument valueDoc(256);
      DeserializationError err = deserializeJson(valueDoc, kv.second.jsonValue);
      if (!err) {
        val["value"] = valueDoc.as<JsonVariant>();
      } else {
        val["value"] = kv.second.jsonValue;
      }
    } else if (kv.second.isNumeric) {
      val["value"] = kv.second.numValue;
    } else {
      val["value"] = kv.second.strValue;
    }

    // Store for next comparison
    lastSentValues[kv.first] = kv.second;
    kv.second.changed = false;
    hasChanges = true;
  }

  if (!hasChanges) return;

  String output;
  serializeJson(doc, output);

  // Debug: Log WebSocket broadcast
  static uint32_t lastDebugLog = 0;
  if (millis() - lastDebugLog > 5000) {  // Log every 5 seconds to avoid spam
    Serial.println("=== WebSocket Broadcast ===");
    Serial.println(output);
    Serial.println("==========================");
    lastDebugLog = millis();
  }

  if (clientSubscriptions.empty()) {
    // Legacy behavior: broadcast to everyone when no one negotiated subscriptions
    ws.textAll(output);
    return;
  }

  // Send to all subscribed clients immediately (no throttling)
  for (auto it = clientSubscriptions.begin(); it != clientSubscriptions.end();) {
    AsyncWebSocketClient* client = ws.client(it->first);
    if (!client) {
      it = clientSubscriptions.erase(it);
      continue;
    }

    client->text(output);
    ++it;
  }
}

// ====== WEBSOCKET MESSAGE HANDLER ======
void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.println("WS: Invalid JSON");
    return;
  }

  String context = doc["context"] | "";
  String subscribe = doc["subscribe"] | "";
  String unsubscribe = doc["unsubscribe"] | "";

  // Handle incoming delta updates from clients (like 6pack app)
  if (doc.containsKey("updates")) {
    Serial.println("\n=== WS: Received delta update from client ===");
    String debugMsg;
    serializeJsonPretty(doc, debugMsg);
    Serial.println(debugMsg);

    // Note: WebSocket delta updates are currently open (no authentication required)
    // This is because the AsyncWebSocket library doesn't provide access to URL parameters
    // where the 6pack app sends its token. Authentication is enforced on PUT requests.

    JsonArray updates = doc["updates"];
    for (JsonVariant updateVar : updates) {
      JsonObject update = updateVar.as<JsonObject>();
      if (!update.containsKey("values")) continue;

      JsonArray values = update["values"];
      for (JsonVariant valueVar : values) {
        JsonObject valueObj = valueVar.as<JsonObject>();
        String path = valueObj["path"] | "";

        if (path.length() == 0) continue;

        // Handle path conversion
        String fullPath = path;
        if (fullPath.startsWith("vessels.self.")) {
          // Remove "vessels.self." prefix
          fullPath.replace("vessels.self.", "");
        } else if (fullPath.startsWith("vessels." + vesselUUID + ".")) {
          // Remove "vessels.<uuid>." prefix
          fullPath.replace("vessels." + vesselUUID + ".", "");
        }
        // If path doesn't start with a known prefix like "navigation.", "environment.", etc.
        // then it's likely a relative path and needs no modification
        // The path from 6pack already includes the full path like "navigation.anchor.akat"

        Serial.printf("WS: Processing delta path: %s -> %s\n", path.c_str(), fullPath.c_str());

        JsonVariant value = valueObj["value"];
        String source = update["source"] | "app";

        // Store the value based on type
        if (value.is<JsonObject>() || value.is<JsonArray>()) {
          String jsonStr;
          serializeJson(value, jsonStr);
          setPathValueJson(fullPath, jsonStr, source, "", "WebSocket update");
          Serial.printf("WS: Stored JSON value for path: %s\n", fullPath.c_str());

          // Special handling for navigation.anchor.akat (6pack app config)
          if (fullPath == "navigation.anchor.akat" && value.is<JsonObject>()) {
            JsonObject obj = value.as<JsonObject>();

            Serial.println("WS: === Processing navigation.anchor.akat update ===");

            // Log current state BEFORE processing
            Serial.printf("WS: Current state - geofence.enabled=%s, depth.enabled=%s, wind.enabled=%s\n",
                          geofence.enabled ? "true" : "false",
                          depthAlarm.enabled ? "true" : "false",
                          windAlarm.enabled ? "true" : "false");

            // Track what's being updated to determine user intent
            bool depthAlarmChanged = false;
            bool windAlarmChanged = false;
            bool anchorPosChanged = false;
            bool radiusChanged = false;

            // Check depth alarm changes
            if (obj.containsKey("depth")) {
              JsonObject depth = obj["depth"];
              if (depth.containsKey("alarm")) {
                bool newDepthEnabled = depth["alarm"];
                if (newDepthEnabled != depthAlarm.enabled) {
                  depthAlarmChanged = true;
                }
              }
              if (depth.containsKey("min_depth")) {
                double newThreshold = depth["min_depth"];
                if (abs(newThreshold - depthAlarm.threshold) > 0.01) {
                  depthAlarmChanged = true;
                }
              }
            }

            // Check wind alarm changes
            if (obj.containsKey("wind")) {
              JsonObject wind = obj["wind"];
              if (wind.containsKey("alarm")) {
                bool newWindEnabled = wind["alarm"];
                if (newWindEnabled != windAlarm.enabled) {
                  windAlarmChanged = true;
                }
              }
              if (wind.containsKey("max_speed")) {
                double newThreshold = wind["max_speed"];
                if (abs(newThreshold - windAlarm.threshold) > 0.01) {
                  windAlarmChanged = true;
                }
              }
            }

            // Process anchor config
            if (obj.containsKey("anchor")) {
              JsonObject anchor = obj["anchor"];

              // Check position change
              if (anchor.containsKey("lat") && anchor.containsKey("lon")) {
                double newLat = anchor["lat"];
                double newLon = anchor["lon"];
                if (abs(newLat - geofence.anchorLat) > 0.00001 || abs(newLon - geofence.anchorLon) > 0.00001) {
                  anchorPosChanged = true;
                  Serial.printf("WS: Anchor position CHANGED: %.6f, %.6f -> %.6f, %.6f\n",
                                geofence.anchorLat, geofence.anchorLon, newLat, newLon);
                }
                geofence.anchorLat = newLat;
                geofence.anchorLon = newLon;
                geofence.anchorTimestamp = millis();
              }

              // Check radius change
              if (anchor.containsKey("radius")) {
                double newRadius = anchor["radius"];
                if (abs(newRadius - geofence.radius) > 0.1) {
                  radiusChanged = true;
                  Serial.printf("WS: Radius CHANGED: %.0f -> %.0f m\n", geofence.radius, newRadius);
                }
                geofence.radius = newRadius;
              }

              // SIMPLE LOGIC: Only update geofence.enabled if:
              // - Anchor position changed, OR
              // - Radius changed, OR
              // - NEITHER depth NOR wind alarm changed (means it's a geofence-only update)
              if (anchor.containsKey("enabled")) {
                bool newEnabled = anchor["enabled"];
                bool isGeofenceUpdate = anchorPosChanged || radiusChanged || (!depthAlarmChanged && !windAlarmChanged);

                Serial.printf("WS: Decision - newEnabled=%s, anchorPos=%s, radius=%s, depth=%s, wind=%s, isGeo=%s\n",
                              newEnabled ? "T" : "F",
                              anchorPosChanged ? "CHG" : "---",
                              radiusChanged ? "CHG" : "---",
                              depthAlarmChanged ? "CHG" : "---",
                              windAlarmChanged ? "CHG" : "---",
                              isGeofenceUpdate ? "YES" : "NO");

                if (isGeofenceUpdate) {
                  if (newEnabled != geofence.enabled) {
                    geofence.enabled = newEnabled;
                    Serial.printf("WS: Geofence enabled CHANGED to %s (geofence update)\n",
                                  geofence.enabled ? "true" : "false");
                  }
                } else {
                  Serial.printf("WS: IGNORING geofence.enabled=%s (depth/wind alarm update)\n",
                                newEnabled ? "true" : "false");
                }
              }
            }

            // Update depth alarm settings
            if (obj.containsKey("depth")) {
              JsonObject depth = obj["depth"];
              if (depth.containsKey("min_depth")) {
                depthAlarm.threshold = depth["min_depth"];
                Serial.printf("WS: Depth threshold: %.1f m\n", depthAlarm.threshold);
              }
              if (depth.containsKey("alarm")) {
                depthAlarm.enabled = depth["alarm"];
                Serial.printf("WS: Depth alarm: %s\n", depthAlarm.enabled ? "ENABLED" : "DISABLED");
              }
            }

            // Update wind alarm settings
            if (obj.containsKey("wind")) {
              JsonObject wind = obj["wind"];
              if (wind.containsKey("max_speed")) {
                windAlarm.threshold = wind["max_speed"];
                Serial.printf("WS: Wind threshold: %.1f kn\n", windAlarm.threshold);
              }
              if (wind.containsKey("alarm")) {
                windAlarm.enabled = wind["alarm"];
                Serial.printf("WS: Wind alarm: %s\n", windAlarm.enabled ? "ENABLED" : "DISABLED");
              }
            }

            // Build the CORRECTED configuration with actual current state
            DynamicJsonDocument correctedDoc(512);
            JsonObject correctedObj = correctedDoc.to<JsonObject>();

            JsonObject anchorObj = correctedObj.createNestedObject("anchor");
            anchorObj["enabled"] = geofence.enabled;  // Use actual state, not what app sent
            anchorObj["radius"] = geofence.radius;
            anchorObj["lat"] = geofence.anchorLat;
            anchorObj["lon"] = geofence.anchorLon;

            JsonObject depthObj = correctedObj.createNestedObject("depth");
            depthObj["alarm"] = depthAlarm.enabled;
            depthObj["min_depth"] = depthAlarm.threshold;

            JsonObject windObj = correctedObj.createNestedObject("wind");
            windObj["alarm"] = windAlarm.enabled;
            windObj["max_speed"] = windAlarm.threshold;

            String correctedJson;
            serializeJson(correctedObj, correctedJson);

            Serial.printf("WS: Persisting corrected config: %s\n", correctedJson.c_str());
            setPathValueJson("navigation.anchor.akat", correctedJson, source, "", "WebSocket update");
          }
        } else if (value.is<double>() || value.is<int>() || value.is<float>()) {
          setPathValue(fullPath, value.as<double>(), source, "", "WebSocket update");
        } else if (value.is<bool>()) {
          setPathValue(fullPath, value.as<bool>() ? 1.0 : 0.0, source, "", "WebSocket update");
        } else if (value.is<const char*>()) {
          setPathValue(fullPath, String(value.as<const char*>()), source, "", "WebSocket update");
        }
      }
    }
    return;
  }

  // Handle subscription
  if (doc.containsKey("subscribe")) {
    JsonArray subArray = doc["subscribe"].as<JsonArray>();
    ClientSubscription& sub = clientSubscriptions[client->id()];

    for (JsonVariant v : subArray) {
      JsonObject subObj = v.as<JsonObject>();
      String path = subObj["path"] | "*";

      if (path == "*") {
        // Subscribe to all
        for (auto& kv : dataStore) {
          sub.paths.insert(kv.first);
        }
      } else {
        sub.paths.insert(path);
      }
    }

    sub.format = doc["format"] | "delta";

    // Send hello message
    DynamicJsonDocument hello(512);
    hello["self"] = "vessels." + vesselUUID;
    hello["version"] = "1.0.0";
    hello["timestamp"] = iso8601Now();

    String helloOutput;
    serializeJson(hello, helloOutput);
    client->text(helloOutput);

    // Send initial state for all subscribed paths
    DynamicJsonDocument initialDoc(4096);
    initialDoc["context"] = "vessels." + vesselUUID;

    JsonArray updates = initialDoc.createNestedArray("updates");
    JsonObject update = updates.createNestedObject();
    update["timestamp"] = iso8601Now();

    JsonObject source = update.createNestedObject("source");
    source["label"] = "ESP32-SignalK";

    JsonArray values = update.createNestedArray("values");
    bool hasData = false;

    // Send current values for subscribed paths
    for (auto& kv : dataStore) {
      // Check if this path is subscribed
      if (sub.paths.count(kv.first) == 0 && sub.paths.count("*") == 0) {
        continue;
      }

      hasData = true;
      JsonObject val = values.createNestedObject();
      val["path"] = kv.first;

      if (kv.second.isJson) {
        DynamicJsonDocument valueDoc(512);
        DeserializationError err = deserializeJson(valueDoc, kv.second.jsonValue);
        if (!err) {
          val["value"] = valueDoc.as<JsonVariant>();
        } else {
          val["value"] = kv.second.jsonValue;
        }
      } else if (kv.second.isNumeric) {
        val["value"] = kv.second.numValue;
      } else {
        val["value"] = kv.second.strValue;
      }
    }

    if (hasData) {
      String initialOutput;
      serializeJson(initialDoc, initialOutput);
      client->text(initialOutput);
      Serial.printf("Sent initial state with %d values to client #%u\n", values.size(), client->id());
    }
  }

  // Handle unsubscribe
  if (doc.containsKey("unsubscribe")) {
    ClientSubscription& sub = clientSubscriptions[client->id()];
    JsonArray unsubArray = doc["unsubscribe"].as<JsonArray>();

    for (JsonVariant v : unsubArray) {
      JsonObject unsubObj = v.as<JsonObject>();
      String path = unsubObj["path"] | "";
      if (path.length() > 0) {
        sub.paths.erase(path);
      }
    }
  }
}

// ====== WEBSOCKET EVENT HANDLER ======
void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                     AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.println("\n========================================");
      Serial.println("=== WEBSOCKET: NEW CONNECTION ===");
      Serial.printf("Client ID: #%u\n", client->id());
      Serial.printf("Client IP: %s\n", client->remoteIP().toString().c_str());
      Serial.printf("Server: %s\n", server->url());

      // Note: Token extraction from URL is not available in AsyncWebSocket
      // The 6pack app sends token in URL query parameter, but we can't access it here
      // For now, clients connect without authentication, and we'll need to handle
      // authentication differently (e.g., through message-level tokens or server config)
      Serial.printf("Client #%u connected (authentication will be checked on write operations)\n", client->id());

      Serial.println("STATUS: CONNECTED SUCCESSFULLY");
      Serial.println("NOTE: WebSocket connections are open - no authentication required");
      Serial.println("      PUT requests require valid tokens via Authorization header");

      // Send hello message immediately
      {
        DynamicJsonDocument helloDoc(512);
        helloDoc["self"] = "vessels." + vesselUUID;
        helloDoc["version"] = "1.7.0";
        helloDoc["timestamp"] = iso8601Now();

        JsonObject serverInfo = helloDoc.createNestedObject("server");
        serverInfo["id"] = serverName;
        serverInfo["version"] = "1.0.0";

        String helloMsg;
        serializeJson(helloDoc, helloMsg);
        client->text(helloMsg);

        Serial.println("Sent hello message:");
        Serial.println(helloMsg);
      }

      Serial.println("========================================\n");
      break;

    case WS_EVT_DISCONNECT:
      Serial.println("\n========================================");
      Serial.printf("=== WEBSOCKET: Client #%u DISCONNECTED ===\n", client->id());
      Serial.println("========================================\n");
      clientSubscriptions.erase(client->id());
      clientTokens.erase(client->id());  // Clean up token
      break;

    case WS_EVT_DATA:
      handleWebSocketMessage(client, data, len);
      break;

    case WS_EVT_ERROR:
      Serial.println("\n========================================");
      Serial.printf("=== WEBSOCKET ERROR: Client #%u ===\n", client->id());
      Serial.println("========================================\n");
      break;

    default:
      break;
  }
}
