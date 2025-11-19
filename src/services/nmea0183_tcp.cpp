#include "nmea0183_tcp.h"
#include "../config.h"

// Forward declaration for NMEA handler defined in main.cpp
extern void handleNmeaSentence(const String& sentence, const char* sourceTag);

// TCP Server instance
static WiFiServer nmeaServer(NMEA_TCP_PORT);

// Client management
struct NMEAClient {
  WiFiClient client;
  uint32_t lastActivity;
  bool active;
  bool allowSend;
  uint16_t sentenceCount;
  uint32_t sentenceWindowStart;
  String rxBuffer;
};

static NMEAClient clients[MAX_NMEA_CLIENTS];
static bool serverStarted = false;
static const char* INPUT_SOURCE_TAG = "NMEA TCP Input";

// Initialize NMEA 0183 TCP server
void initNMEA0183Server() {
  Serial.println("\n=== Initializing NMEA 0183 TCP Server ===");

  // Initialize client array
  for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
    clients[i].active = false;
    clients[i].lastActivity = 0;
    clients[i].allowSend = true;
    clients[i].sentenceCount = 0;
    clients[i].sentenceWindowStart = 0;
    clients[i].rxBuffer = "";
  }

  // Start TCP server
  nmeaServer.begin();
  nmeaServer.setNoDelay(true);  // Disable Nagle algorithm for low latency

  serverStarted = true;

  Serial.printf("✅ NMEA 0183 TCP Server started on port %d\n", NMEA_TCP_PORT);
  Serial.printf("   Max clients: %d\n", MAX_NMEA_CLIENTS);
  Serial.printf("   Client timeout: %d ms\n", CLIENT_TIMEOUT_MS);
  Serial.println("======================================\n");
}

// Process TCP server - accept new clients and handle disconnections
void processNMEA0183Server() {
  if (!serverStarted) return;

  uint32_t now = millis();

  // Check for new client connections
  WiFiClient newClient = nmeaServer.available();
  if (newClient) {
    // Find free slot
    int freeSlot = -1;
    for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
      if (!clients[i].active) {
        freeSlot = i;
        break;
      }
    }

    if (freeSlot >= 0) {
      clients[freeSlot].client = newClient;
      clients[freeSlot].client.setNoDelay(true);
      clients[freeSlot].lastActivity = now;
      clients[freeSlot].active = true;
      clients[freeSlot].allowSend = true;
      clients[freeSlot].sentenceCount = 0;
      clients[freeSlot].sentenceWindowStart = now;
      clients[freeSlot].rxBuffer = "";

      Serial.printf("NMEA TCP: New client [%d] connected from %s\n",
                    freeSlot, newClient.remoteIP().toString().c_str());
    } else {
      // No free slots - reject connection
      Serial.println("NMEA TCP: Max clients reached, rejecting connection");
      newClient.stop();
    }
  }

  // Check existing clients for disconnections and timeouts
  for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
    if (!clients[i].active) continue;

    // Check if client is still connected
    if (!clients[i].client.connected()) {
      Serial.printf("NMEA TCP: Client [%d] disconnected\n", i);
      clients[i].client.stop();
      clients[i].active = false;
      continue;
    }

    // Process inbound data (for mock feeds)
    bool hadData = false;
    while (clients[i].client.available()) {
      hadData = true;
      clients[i].lastActivity = now;

      char c = clients[i].client.read();
      if (c == '\n' || c == '\r') {
        if (clients[i].rxBuffer.length() > 6 && clients[i].rxBuffer[0] == '$') {
          if (now - clients[i].sentenceWindowStart > 1000) {
            clients[i].sentenceWindowStart = now;
            clients[i].sentenceCount = 0;
          }

          if (clients[i].sentenceCount >= MAX_SENTENCES_PER_SECOND) {
            Serial.printf("NMEA TCP: Client [%d] exceeded rate limit, disconnecting\n", i);
            clients[i].client.stop();
            clients[i].active = false;
            clients[i].rxBuffer = "";
            break;
          }

          clients[i].sentenceCount++;
          clients[i].allowSend = false;  // Don't echo data back to this client
          handleNmeaSentence(clients[i].rxBuffer, INPUT_SOURCE_TAG);
          yield();
        }
        clients[i].rxBuffer = "";
      } else if (c >= 32 && c <= 126) {
        if (clients[i].rxBuffer.length() < 120) {
          clients[i].rxBuffer += c;
        } else {
          clients[i].rxBuffer = "";
          Serial.printf("NMEA TCP: Client [%d] payload overflow, resetting buffer\n", i);
        }
      }
    }

    if (!hadData && (now - clients[i].lastActivity > CLIENT_TIMEOUT_MS)) {
      Serial.printf("NMEA TCP: Client [%d] timeout, disconnecting\n", i);
      clients[i].client.stop();
      clients[i].active = false;
      continue;
    }
  }
}

// Broadcast NMEA 0183 sentence to all connected clients
void broadcastNMEA0183(const String& sentence) {
  if (!serverStarted || sentence.length() == 0) return;

  uint32_t now = millis();
  int sentCount = 0;

  for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
    if (!clients[i].active || !clients[i].allowSend) continue;

    if (clients[i].client.connected()) {
      // Try to send
      size_t written = clients[i].client.write(sentence.c_str(), sentence.length());

      if (written == sentence.length()) {
        clients[i].lastActivity = now;
        sentCount++;
      } else {
        // Write failed - mark for disconnect
        Serial.printf("NMEA TCP: Client [%d] write failed, disconnecting\n", i);
        clients[i].client.stop();
        clients[i].active = false;
      }
    } else {
      // Not connected anymore
      clients[i].client.stop();
      clients[i].active = false;
    }
  }

  // Debug output (throttled)
  static uint32_t lastDebug = 0;
  if (sentCount > 0 && (now - lastDebug > 10000)) {
    lastDebug = now;
    Serial.printf("NMEA TCP: Broadcasting to %d clients\n", sentCount);
  }
}

// Get number of connected clients
int getNMEA0183ClientCount() {
  int count = 0;
  for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
    if (clients[i].active && clients[i].client.connected()) {
      count++;
    }
  }
  return count;
}

// Stop NMEA 0183 TCP server
void stopNMEA0183Server() {
  if (!serverStarted) return;

  Serial.println("NMEA TCP: Stopping server...");

  // Disconnect all clients
  for (int i = 0; i < MAX_NMEA_CLIENTS; i++) {
    if (clients[i].active) {
      clients[i].client.stop();
      clients[i].active = false;
    }
    clients[i].allowSend = true;
    clients[i].sentenceCount = 0;
    clients[i].sentenceWindowStart = 0;
    clients[i].rxBuffer = "";
  }

  nmeaServer.stop();
  serverStarted = false;

  Serial.println("NMEA TCP: Server stopped");
}
