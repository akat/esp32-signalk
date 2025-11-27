/*
  ESP32 SignalK Server - Refactored Main Entry Point

  This is the main entry point for the ESP32 SignalK server application.
  All functionality has been modularized into separate components.

  Features:
  - WiFiManager for easy network setup
  - Full SignalK REST API and WebSocket support
  - NMEA0183 and NMEA2000 parsing
  - I2C sensor support (BME280)
  - Geofence and alarm monitoring
  - Expo push notifications
  - Token-based authentication

  Hardware Support:
  - LILYGO TTGO T-CAN485 ESP32 board
  - CAN bus for NMEA2000
  - RS485 or GPIO UART for NMEA0183
  - I2C sensors (BME280)
  - GPS module via Serial2
*/

// ====== ARDUINO & ESP32 CORE ======
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <Wire.h>
#include <time.h>

// ====== STANDARD LIBRARY ======
#include <map>
#include <set>
#include <vector>

// ====== THIRD PARTY LIBRARIES ======
#include <ArduinoJson.h>
#include <Adafruit_BME280.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>
#include <SoftwareSerial.h>
#include <driver/uart.h>  // ESP32 native UART driver for inverted mode support

// ====== PROJECT CONFIGURATION ======
#include "config.h"
#include "types.h"

// ====== UTILITY MODULES ======
#include "utils/uuid.h"
#include "utils/time_utils.h"
#include "utils/conversions.h"
#include "utils/nmea0183_converter.h"

// ====== SIGNALK DATA MODULES ======
#include "signalk/globals.h"
#include "signalk/data_store.h"

// ====== SERVICE MODULES ======
#include "services/storage.h"
#include "services/expo_push.h"
#include "services/alarms.h"
#include "services/websocket.h"
#include "services/nmea0183_tcp.h"
#include "services/dyndns.h"

// ====== HARDWARE MODULES ======
#include "hardware/nmea0183.h"
#include "hardware/nmea2000.h"
#include "hardware/sensors.h"
#include "hardware/led_status.h"
#include "hardware/seatalk1.h"

// ====== API MODULES ======
#include "api/security.h"
#include "api/handlers.h"
#include "api/routes.h"
#include "api/web_auth.h"

// ====== FORWARD DECLARATIONS ======
// Declare functions that will be used by hardware modules
bool shouldBroadcastFromSource(const char* sourceTag);

// ====== GLOBAL VARIABLE DEFINITIONS ======
// These are declared as 'extern' in various header files and defined here

// Server infrastructure
AsyncWebServer server(3000);  // SignalK default port
AsyncWebSocket ws("/signalk/v1/stream");
Preferences prefs;
WiFiManager wm;

// WebSocket client management
std::map<uint32_t, ClientSubscription> clientSubscriptions;
std::map<uint32_t, String> clientTokens;

// SignalK data storage
std::map<String, PathValue> dataStore;
std::map<String, PathValue> lastSentValues;
std::map<String, String> notifications;

// Vessel identification
String vesselUUID;
String serverName = "ESP32-SignalK";

// Access request system
std::map<String, AccessRequestData> accessRequests;
std::map<String, ApprovedToken> approvedTokens;

// Expo push notification tokens
std::vector<String> expoTokens;
unsigned long lastPushNotification = 0;

// GPS and navigation data
GPSData gpsData;

// TCP broadcast priority tracking - ONE active source broadcasts at a time
// This prevents duplicate data (e.g., position, depth) from multiple sources
struct TcpSourceInfo {
  uint32_t lastSeen;   // millis when last sentence received from this source
  int rank;            // lower = higher priority
};
static std::map<String, TcpSourceInfo> tcpSources;
static String activeTcpSource = "";  // Currently broadcasting source (global for all sentences)
static const uint32_t TCP_STALE_MS = 10000;  // 10s fallback window

// Alarm configurations
GeofenceConfig geofence;
DepthAlarmConfig depthAlarm;
WindAlarmConfig windAlarm;
DynDnsConfig dynDnsConfig;

// TCP Client for external SignalK server
WiFiClient tcpClient;
String tcpServerHost = "";
int tcpServerPort = 10110;
bool tcpEnabled = false;
uint32_t lastTcpReconnect = 0;
uint32_t tcpConnectedTime = 0;
String tcpBuffer = "";
TcpClientState tcpState = TCP_DISCONNECTED;

// Map source tag to dataStore source label
static String sourceTagToLabel(const String& tag) {
  String t = tag;
  t.toLowerCase();
  if (t.indexOf("nmea2000") >= 0 || t.indexOf("can") >= 0) return "nmea2000.can";
  if (t.indexOf("rs485") >= 0) return "nmea0183.rs485";
  if (t.indexOf("single") >= 0) return "nmea0183.singleended";
  if (t.indexOf("seatalk") >= 0) return "seatalk1";
  if (t.indexOf("gps") >= 0 || tag.length() == 0) return "nmea0183.GPS";  // nullptr maps to GPS
  if (t.indexOf("tcp") >= 0) return "nmea0183.tcp";
  return "nmea0183.GPS";  // default
}

// Priority rank for TCP broadcast (lower = higher priority)
static int tcpPriorityRank(const String& src) {
  String s = src;
  s.toLowerCase();
  if (s.indexOf("nmea2000") >= 0 || s.indexOf("can") >= 0) return 0;
  if (s.indexOf("rs485") >= 0) return 1;
  if (s.indexOf("single") >= 0) return 2;
  if (s.indexOf("seatalk") >= 0) return 3;
  if (s.indexOf("gps") >= 0) return 4;
  if (s.indexOf("tcp") >= 0) return 5;
  return 6;
}

// Helper function to check if a source should broadcast to TCP
// Returns true if this source has the highest priority among all active sources
bool shouldBroadcastFromSource(const char* sourceTag) {
  String tag = sourceTag ? String(sourceTag) : "";
  String myLabel = sourceTagToLabel(tag);
  int myRank = tcpPriorityRank(tag);

  // Check dataStore to find the best (lowest rank) source that has data
  extern std::map<String, PathValue> dataStore;

  int bestRank = 999;
  for (const auto& pair : dataStore) {
    const PathValue& pv = pair.second;
    int rank = tcpPriorityRank(pv.source);
    if (rank < bestRank) {
      bestRank = rank;
    }
  }

  // Broadcast only if this source has the best rank
  return (myRank <= bestRank);
}

// Central handler to keep NMEA inputs consistent across all sources
void handleNmeaSentence(const String& sentence, const char* sourceTag) {
  if (sentence.length() < 7 || sentence[0] != '$') {
    return;
  }

  String sourceLabel = (sourceTag != nullptr) ? String(sourceTag) : "unknown";
  int rank = tcpPriorityRank(sourceLabel);
  uint32_t now = millis();

  // Update this source's last seen timestamp and rank
  TcpSourceInfo srcInfo;
  srcInfo.lastSeen = now;
  srcInfo.rank = rank;
  tcpSources[sourceLabel] = srcInfo;

  // Determine which source should be active for TCP broadcasting
  // Rule: Use the highest priority (lowest rank) source that is NOT stale
  String bestSource = "";
  int bestRank = 999;

  for (auto& pair : tcpSources) {
    const String& src = pair.first;
    const TcpSourceInfo& info = pair.second;

    // Skip stale sources
    if ((now - info.lastSeen) > TCP_STALE_MS) continue;

    // Find best (lowest rank = highest priority)
    if (info.rank < bestRank) {
      bestRank = info.rank;
      bestSource = src;
    }
  }

  // Update active source if changed
  if (bestSource != activeTcpSource) {
    if (activeTcpSource.length() > 0) {
      Serial.printf("TCP Priority: Switching from %s to %s (rank %d)\n",
                   activeTcpSource.c_str(), bestSource.c_str(), bestRank);
    } else {
      Serial.printf("TCP Priority: Activating %s (rank %d)\n",
                   bestSource.c_str(), bestRank);
    }
    activeTcpSource = bestSource;
  }

  // ALWAYS parse sentence to update SignalK data store from ALL sources
  parseNMEASentence(sentence);

  // NOTE: We do NOT forward raw sentences anymore!
  // Instead, NMEA sentences are generated from the dataStore periodically
  // in the main loop. This ensures per-path priority filtering.
}

// Serial port buffers
String gpsBuffer = "";
String nmeaBuffer = "";
String singleEndedBuffer = "";

// GPS on SoftwareSerial (moved from UART2 to allow Single-Ended NMEA to use UART2)
#include <SoftwareSerial.h>
SoftwareSerial gpsSerial;

// Single-Ended NMEA 0183 (Direct connection with inverted mode support for optocouplers)
#ifdef USE_SINGLEENDED_NMEA
  // Using HardwareSerial with inverted RX mode (native ESP32 UART feature)
  // Uses UART2 (GPS moved to SoftwareSerial to free up UART2)
  HardwareSerial SingleEndedSerial(2);  // Use UART2 (independent from RS485)
  bool singleEndedUseInverted = true;  // Enable inverted mode for optocoupler
#endif

// NMEA2000 CAN Bus state
bool n2kEnabled = false;

// I2C Sensors
Adafruit_BME280 bme;
bool bmeEnabled = false;
unsigned long lastSensorRead = 0;

// ====== TCP CLIENT HELPER FUNCTIONS ======
// These functions manage the TCP client connection to external SignalK servers

void connectToTcpServer() {
  if (!tcpEnabled || tcpServerHost.length() == 0) {
    tcpState = TCP_DISCONNECTED;
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastWifiWarning = 0;
    uint32_t now = millis();
    if (now - lastWifiWarning > 30000) { // Log every 30 seconds
      Serial.println("TCP: Cannot connect - WiFi not connected to internet");
      lastWifiWarning = now;
    }
    tcpState = TCP_ERROR;
    return;
  }

  if (tcpClient.connected()) {
    tcpState = TCP_CONNECTED;
    return; // Already connected
  }

  uint32_t now = millis();
  if (now - lastTcpReconnect < TCP_RECONNECT_DELAY) {
    return; // Wait before reconnecting
  }

  lastTcpReconnect = now;
  tcpState = TCP_CONNECTING;

  Serial.println("\n=== Connecting to TCP Server ===");
  Serial.println("Host: " + tcpServerHost);
  Serial.println("Port: " + String(tcpServerPort));

  // Try DNS resolution first
  IPAddress serverIP;
  if (WiFi.hostByName(tcpServerHost.c_str(), serverIP)) {
    Serial.println("DNS resolved to: " + serverIP.toString());

    if (tcpClient.connect(serverIP, tcpServerPort, 5000)) { // 5 second timeout
      Serial.println("TCP: Connected successfully!");
      Serial.println("Remote IP: " + tcpClient.remoteIP().toString());
      tcpBuffer = "";
      tcpConnectedTime = millis();
      tcpState = TCP_CONNECTED;
    } else {
      Serial.println("TCP: Connection failed (connection refused or timeout)");
      tcpState = TCP_ERROR;
    }
  } else {
    Serial.println("TCP: DNS resolution failed for " + tcpServerHost);
    Serial.println("Check hostname and ensure DNS is working");
    tcpState = TCP_ERROR;
  }
}

void processTcpData() {
  if (!tcpEnabled || !tcpClient.connected()) {
    if (tcpState == TCP_CONNECTED) {
      tcpState = TCP_DISCONNECTED;
      Serial.println("TCP: Connection lost");
    }
    return;
  }

  // Read available data - process NMEA sentences from TCP
  while (tcpClient.available()) {
    char c = tcpClient.read();

    // NMEA sentences end with newline
      if (c == '\n' || c == '\r') {
        if (tcpBuffer.length() > 0) {
          if (tcpBuffer[0] == '$') {
            // This is an NMEA sentence - parse it
            handleNmeaSentence(tcpBuffer, nullptr);
          } else {
            // Non-NMEA data - just log it for debugging
            Serial.println("TCP Data (non-NMEA): " + tcpBuffer);
          }
        }
      tcpBuffer = "";
    } else if (c >= 32 && c <= 126) {
      // Only accept printable ASCII characters
      if (tcpBuffer.length() < 120) { // NMEA max length is typically 82 chars
        tcpBuffer += c;
      } else {
        // Buffer overflow protection - reset
        tcpBuffer = "";
        Serial.println("TCP: Buffer overflow, resetting");
      }
    }
  }

  // Check if connection is still alive
  if (!tcpClient.connected()) {
    Serial.println("TCP: Connection lost");
    tcpBuffer = "";
    tcpState = TCP_DISCONNECTED;
  }
}

// ====== SETUP FUNCTION ======

// WiFi Reset Handler (called from API routes)
void handleWiFiReset(AsyncWebServerRequest* req) {
  Serial.println("WiFi reset requested via API");
  req->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi settings reset. Device restarting...\"}");
  delay(1000);
  wm.resetSettings();
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 SignalK Server ===\n");
  Serial.println("Firmware compiled with NMEA TCP server support");
  Serial.println("Ready to receive NMEA data on port 10110\n");

  // Initialize LED status indicators first
  Serial.println("Initializing LED status indicators...");
  initLEDs();

  // Early debug output to verify we're running
  Serial.println("Setup starting...");

  // Test basic functionality first
  Serial.println("Testing basic operations...");
  String test = "test";
  Serial.println("String operations work: " + test);
  Serial.println("Basic operations test passed");

  // Load approved tokens from flash
  Serial.println("Loading approved tokens...");
  loadApprovedTokens();

  // Load Expo push tokens from flash
  Serial.println("Loading Expo push tokens...");
  loadExpoTokens();

  // Load or generate vessel UUID
  Serial.println("Loading preferences...");
  Serial.println("Creating Preferences object...");
  Preferences testPrefs;
  Serial.println("Preferences object created");
  testPrefs.begin("signalk", false);
  Serial.println("Preferences namespace opened");
  vesselUUID = testPrefs.getString("vessel_uuid", "");
  Serial.println("Read vessel_uuid from prefs");
  if (vesselUUID.length() == 0) {
    Serial.println("Generating new UUID...");
    vesselUUID = generateUUID();
    Serial.println("UUID generated: " + vesselUUID);
    testPrefs.putString("vessel_uuid", vesselUUID);
    Serial.println("UUID saved to preferences");
  }
  serverName = testPrefs.getString("server_name", "ESP32-SignalK");
  Serial.println("Read server_name from prefs");

  // Restore persisted SignalK paths
  Serial.println("Checking for persisted anchor.akat...");
  String akatData = testPrefs.getString("anchor.akat", "");
  Serial.printf("Read %d bytes from flash\n", akatData.length());

  if (akatData.length() > 0) {
    Serial.println("Restoring navigation.anchor.akat from flash...");
    Serial.printf("Data: %s\n", akatData.c_str());
    setPathValueJson("navigation.anchor.akat", akatData, "persisted", "", "Restored from flash");

    // Parse and apply the configuration
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, akatData);
    if (!err && doc.is<JsonObject>()) {
      JsonObject obj = doc.as<JsonObject>();

      // Extract anchor position
      bool anchorPositionUpdated = false;
      if (obj.containsKey("anchor")) {
        JsonObject anchor = obj["anchor"];
        if (anchor.containsKey("lat") && anchor.containsKey("lon")) {
          geofence.anchorLat = anchor["lat"];
          geofence.anchorLon = anchor["lon"];
          geofence.anchorTimestamp = millis();
          anchorPositionUpdated = true;
          Serial.printf("Restored anchor: %.6f, %.6f\n", geofence.anchorLat, geofence.anchorLon);
        }
        if (anchor.containsKey("radius")) {
          geofence.radius = anchor["radius"];
          Serial.printf("Restored radius: %.0f m\n", geofence.radius);
        }
         // IMPORTANT: Only update geofence.enabled if anchor position is also being set
        // This prevents depth/wind alarm updates from accidentally enabling geofence
        if (anchor.containsKey("enabled") && anchorPositionUpdated) {
          bool newEnabled = anchor["enabled"];
          if (newEnabled != geofence.enabled) {
            geofence.enabled = newEnabled;
            Serial.printf("Geofence enabled changed to: %s\n", geofence.enabled ? "true" : "false");
          } else {
            Serial.printf("Geofence enabled unchanged: %s\n", geofence.enabled ? "true" : "false");
          }
        } else if (anchor.containsKey("enabled") && !anchorPositionUpdated) {
          Serial.printf("Ignoring geofence.enabled update - no anchor position in request\n");
        }
      }

      // Extract depth alarm config
      if (obj.containsKey("depth")) {
        JsonObject depth = obj["depth"];
        if (depth.containsKey("min_depth")) {
          depthAlarm.threshold = depth["min_depth"];
          Serial.printf("Restored depth threshold: %.1f m\n", depthAlarm.threshold);
        }
        if (depth.containsKey("alarm")) {
          bool newEnabled = depth["alarm"];
          if (newEnabled != depthAlarm.enabled) {
            depthAlarm.enabled = newEnabled;
            Serial.printf("Depth alarm enabled changed to: %s\n", depthAlarm.enabled ? "true" : "false");
          } else {
            Serial.printf("Depth alarm enabled unchanged: %s\n", depthAlarm.enabled ? "true" : "false");
          }
        }
      }

      // Extract wind alarm config
      if (obj.containsKey("wind")) {
        JsonObject wind = obj["wind"];
        if (wind.containsKey("max_speed")) {
          windAlarm.threshold = wind["max_speed"];
          Serial.printf("Restored wind threshold: %.1f kn\n", windAlarm.threshold);
        }
        if (wind.containsKey("alarm")) {
          bool newEnabled = wind["alarm"];
          if (newEnabled != windAlarm.enabled) {
            windAlarm.enabled = newEnabled;
            Serial.printf("Wind alarm enabled changed to: %s\n", windAlarm.enabled ? "true" : "false");
          } else {
            Serial.printf("Wind alarm enabled unchanged: %s\n", windAlarm.enabled ? "true" : "false");
          }
        }
      }
    }
  } else {
    Serial.println("No persisted anchor.akat data found");
  }

  testPrefs.end();
  Serial.println("Preferences closed successfully");

  // Load TCP configuration
  loadTcpConfig();
  loadDynDnsConfig();
  loadHardwareConfig();
  loadAPConfig();

  // Start WiFi in AP+STA mode (both Access Point and Station)
  // This keeps the AP running even when connected to another WiFi network
  Serial.println("\n=== Starting WiFi Access Point ===");
  Serial.println("Setting WiFi mode...");
  WiFi.mode(WIFI_AP_STA);  // AP + Station mode
  Serial.println("WiFi mode set to AP+STA");

  // Configure AP with static IP
  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),    // AP IP address
    IPAddress(192, 168, 4, 1),    // Gateway
    IPAddress(255, 255, 255, 0)   // Subnet mask
  );

  Serial.println("Starting softAP...");
  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_HIDDEN, AP_MAX_CONNECTIONS);

  if (!apStarted) {
    Serial.println("Failed to start Access Point, restarting...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("SoftAP started successfully");

  Serial.println("SSID: " + String(AP_SSID));
  Serial.println("Password: " + String(AP_PASSWORD));
  Serial.println("AP IP Address: " + WiFi.softAPIP().toString());
  Serial.println("AP MAC Address: " + WiFi.softAPmacAddress());
  Serial.println("==============================\n");

  // Start mDNS
  Serial.println("Starting mDNS...");
  if (MDNS.begin("esp32-signalk")) {
    Serial.println("mDNS responder started");
    Serial.println("Hostname: esp32-signalk.local");
    MDNS.addService("http", "tcp", 3000);
    MDNS.addService("signalk-http", "tcp", 3000);
    MDNS.addService("signalk-ws", "tcp", 3000);
  } else {
    Serial.println("Error starting mDNS");
  }

  // NMEA UART (Serial1)
  Serial.println("Starting NMEA UART...");

  #ifdef USE_RS485_FOR_NMEA
    // Configure RS485 control pins
    pinMode(NMEA_DE, OUTPUT);
    digitalWrite(NMEA_DE, LOW);  // Receive mode (DE/RE low)
    pinMode(NMEA_DE_ENABLE, OUTPUT);
    digitalWrite(NMEA_DE_ENABLE, LOW);  // Enable chip (inverted logic)

    Serial1.begin(NMEA_BAUD, SERIAL_8N1, NMEA_RX, NMEA_TX);
    Serial.println("\n=== RS485 Configuration ===");
    Serial.println("NMEA0183 via RS485 started on terminal blocks A/B");
    Serial.println("Using built-in RS485 transceiver (GPIO 21/22)");
    Serial.printf("Baud rate: %d (Common: 4800 or 9600)\n", NMEA_BAUD);
    Serial.printf("DE pin (GPIO %d): LOW (Receive mode)\n", NMEA_DE);
    Serial.printf("DE_ENABLE pin (GPIO %d): LOW (Chip enabled)\n", NMEA_DE_ENABLE);
    Serial.println("\nDEPTH SOUNDER WIRING:");
    Serial.println("  Terminal A (Blue)   -> RS485 Data+");
    Serial.println("  Terminal GND (Black)-> Ground");
    Serial.println("  Terminal B (White)  -> RS485 Data-");
    Serial.println("\nNOTE: If no data received, try:");
    Serial.println("  1. Swap A/B wires (reversed polarity)");
    Serial.println("  2. Change baud rate to 9600 in config.h");
    Serial.println("  3. Check depth sounder power");
    Serial.println("\nWaiting for NMEA sentences ($SDDBT, $SDDPT)...");
    Serial.println("===========================\n");
  #else
    Serial1.begin(NMEA_BAUD, SERIAL_8N1, NMEA_RX, NMEA_TX);
    Serial.println("NMEA0183 UART started on GPIO header pins RX:" + String(NMEA_RX) + " TX:" + String(NMEA_TX));
  #endif

  // GPS Module (SoftwareSerial)
  // NOTE: GPS moved from UART2 to SoftwareSerial to allow Single-Ended NMEA to use UART2
  Serial.println("Starting GPS module...");
  gpsSerial.begin(hardwareConfig.gps_baud, SWSERIAL_8N1, hardwareConfig.gps_rx, hardwareConfig.gps_tx, false, 256);
  Serial.printf("GPS SoftwareSerial started on pins RX:%d TX:%d @ %d baud\n",
    hardwareConfig.gps_rx, hardwareConfig.gps_tx, hardwareConfig.gps_baud);
  Serial.println("Note: GPS now uses SoftwareSerial (UART2 reserved for Single-Ended NMEA)");

  // Initialize Seatalk 1 (if enabled)
  #ifdef USE_SEATALK1
    Serial.println("\n=== Seatalk 1 Initialization ===");
    if (initSeatalk1(SEATALK1_RX)) {
      Serial.println("Seatalk 1 initialized successfully");
      setSeatalk1Debug(true);  // Enable debug output
    } else {
      Serial.println("Failed to initialize Seatalk 1");
    }
    Serial.println("================================\n");
  #endif

  // Initialize Single-Ended NMEA 0183 (Direct connection - not RS485)
  #ifdef USE_SINGLEENDED_NMEA
    Serial.println("\n=== Single-Ended NMEA 0183 Input ===");
    Serial.println("IMPORTANT: GPIO 33 requires voltage divider OR optocoupler isolation");
    Serial.println("Wiring: NMEA OUT → 10kΩ → GPIO 33 → 3.9kΩ → GND");
    Serial.println("        OR: NMEA OUT → Optocoupler → GPIO 33");
    Serial.println("This converts 12V NMEA signal to safe 3.3V");
    Serial.println("");

    // Initialize HardwareSerial with custom RX pin and inverted mode
    // This allows using optocouplers without hardware inverter
    SingleEndedSerial.begin(hardwareConfig.singleended_baud, SERIAL_8N1,
                            hardwareConfig.singleended_rx, -1,  // RX only, no TX
                            singleEndedUseInverted);  // Enable inverted RX mode

    Serial.printf("Single-Ended NMEA initialized on GPIO %d @ %d baud (UART2)\n",
                  hardwareConfig.singleended_rx, hardwareConfig.singleended_baud);
    Serial.println("Mode: Hardware RX inversion enabled for optocoupler compatibility");
    Serial.println("✅ All peripherals active: RS485 (UART1) + Single-Ended (UART2) + GPS (SoftwareSerial)");
    Serial.println("Waiting for NMEA sentences (wind, depth, etc.)...");
    Serial.println("====================================\n");
  #endif

  // Initialize NMEA2000 CAN Bus
  initNMEA2000();

  // Initialize I2C Sensors
  initI2CSensors();

  // Initialize NMEA 0183 TCP Server (port 10110)
  initNMEA0183Server();
  initDynDnsService();

  // HTTP GET handler for /signalk/v1/stream - Token validation for SensESP
  // IMPORTANT: This MUST be registered BEFORE the WebSocket handler!
  // SensESP v3.1.1 validates tokens by making GET request to /signalk/v1/stream
  // and expects HTTP 426 "Upgrade Required" to indicate valid token
  server.on("/signalk/v1/stream", HTTP_GET, [](AsyncWebServerRequest* req) {
    // Check for Authorization header
    if (req->hasHeader("Authorization")) {
      String auth = req->header("Authorization");

      // Extract token from "Bearer <token>" format
      if (auth.startsWith("Bearer ")) {
        String token = auth.substring(7);
        token.trim();

        // Check if token is in approved list
        if (approvedTokens.find(token) != approvedTokens.end()) {
          // Return 426 Upgrade Required - this tells SensESP the token is valid
          // and it should upgrade to WebSocket connection
          req->send(426, "text/plain", "Upgrade Required");
          return;
        } else {
          req->send(401, "text/plain", "Unauthorized");
          return;
        }
      }
    }

    req->send(401, "text/plain", "Unauthorized");
  });

  // Initialize web authentication
  Serial.println("Initializing web authentication...");
  initWebAuth();
  Serial.println("Web authentication initialized");

  // WebSocket setup
  Serial.println("Setting up WebSocket...");
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  Serial.println("WebSocket setup complete");

  // Setup all HTTP routes via the routes module
  Serial.println("Setting up HTTP routes...");
  setupRoutes(server);
  Serial.println("HTTP routes configured");

  // Reboot endpoint
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html",
      "<html><body><h2>Rebooting device...</h2>"
      "<p>Please wait 10 seconds then refresh.</p>"
      "<script>setTimeout(function(){window.location.href='/';}, 10000);</script>"
      "</body></html>");
    delay(100);
    ESP.restart();
  });

  Serial.println("Starting HTTP server...");
  server.begin();
  Serial.println("\nHTTP Server started");

  Serial.println("\n========================================");
  Serial.println("=== SIGNALK SERVER READY ===");
  Serial.println("========================================");
  Serial.println("Server running on port 3000");
  Serial.println("\n--- For SensESP Connection ---");
  Serial.println("1. SensESP should POST to: /signalk/v1/access/requests");
  Serial.println("2. Will receive 202 + requestId");
  Serial.println("3. SensESP polls: /signalk/v1/access/requests/{requestId}");
  Serial.println("4. Will receive token");
  Serial.println("5. SensESP connects to: ws://IP:3000/signalk/v1/stream");
  Serial.println("\n--- Discovery Endpoint ---");
  Serial.println("GET /signalk returns WebSocket URL with :3000 port");
  Serial.println("========================================\n");

  Serial.println("\n=== Access URLs ===");
  Serial.println("SignalK Server: http://192.168.4.1:3000/");
  Serial.println("SignalK API:    http://192.168.4.1:3000/signalk/v1/api/");
  Serial.println("WebSocket:      ws://192.168.4.1:3000/signalk/v1/stream");
  Serial.println("\n=== Connect to WiFi: " + String(AP_SSID) + " ===");
  Serial.println("Password: " + String(AP_PASSWORD));
  Serial.println("=====================================\n");

  // Start WiFiManager config portal (non-blocking)
  Serial.println("\n=== Starting WiFiManager ===");
  Serial.println("WiFi Config Portal: http://192.168.4.1");
  Serial.println("Connect to configure WiFi network for internet access");
  Serial.println("============================\n");

  // Configure WiFiManager
  wm.setDebugOutput(true);
  wm.setConfigPortalBlocking(false);  // Non-blocking mode
  wm.setConfigPortalTimeout(0);       // Never timeout - always available
  wm.setConnectTimeout(15);           // Connection attempt timeout (reduced to 15s)
  wm.setConnectRetries(3);            // Maximum 3 connection attempts
  wm.setWiFiAutoReconnect(true);      // Auto reconnect to saved network
  wm.setSaveConfigCallback([]() {
    Serial.println("WiFi credentials saved!");
  });

  // Try to connect with saved credentials
  Serial.println("Attempting to connect with saved credentials...");
  Serial.println("Will timeout after 15 seconds if connection fails");

  // autoConnect will try saved credentials and start portal if it fails
  // It will automatically timeout after the configured time
  bool connected = wm.autoConnect(AP_SSID, AP_PASSWORD);

  // Wait for connection attempt with timeout
  Serial.print("Connecting");
  int connectAttempts = 0;
  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && connectAttempts < 30 && (millis() - startTime) < 20000) {
    delay(500);
    Serial.print(".");
    connectAttempts++;
  }
  Serial.println();

  // If stuck in association error, force disconnect
  if (WiFi.status() != WL_CONNECTED && connectAttempts >= 30) {
    Serial.println("Connection timeout - forcing disconnect");
    WiFi.disconnect(true);
    delay(1000);
  }

  // Now start the web portal manually to ensure it's always available
  // This keeps both AP and web portal running even when STA is connected
  if (!wm.getWebPortalActive()) {
    Serial.println("Starting web portal...");
    wm.startWebPortal();
  }

  // Check connection status
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n=== WiFi Client Connected ===");
    Serial.println("Connected to: " + WiFi.SSID());
    Serial.println("IP Address: " + WiFi.localIP().toString());
    Serial.println("============================\n");

    // Sync time via NTP
    Serial.println("Syncing time with NTP...");
    configTime(0, 0, "pool.ntp.org", "time.ntp.org");
    int timeRetries = 0;
    while (time(nullptr) < 100000 && timeRetries < 20) {
      delay(500);
      Serial.print(".");
      timeRetries++;
    }
    Serial.println(time(nullptr) > 100000 ? " OK" : " FAILED");
  } else {
    Serial.println("\n=== WiFi Not Connected ===");
    Serial.println("Could not connect with saved credentials");
    Serial.println("Please configure WiFi via portal");
    Serial.println("============================\n");
  }

  // Show both access points
  Serial.println("\n=== WiFiManager Portal ===");
  Serial.println("Portal: http://192.168.4.1");
  Serial.println("SSID: " + String(AP_SSID));
  Serial.println("Pass: " + String(AP_PASSWORD));
  Serial.println("==========================\n");

  Serial.println("\n=== System Ready ===\n");
}

// ====== MAIN LOOP ======
uint32_t lastWsCleanup = 0;
uint32_t lastWsPing = 0;
uint32_t lastStatusLog = 0;
uint32_t lastWifiCheck = 0;
uint32_t lastWifiReconnect = 0;
uint32_t lastSessionCleanup = 0;
bool wasWifiConnected = false;
int wifiReconnectAttempts = 0;

// Generate NMEA 0183 sentences from SignalK dataStore
// This ensures per-path priority filtering - only the best source for each path
void generateNmeaFromDataStore() {
  extern std::map<String, PathValue> dataStore;

  static uint32_t debugCounter = 0;
  if (debugCounter++ % 10 == 0) {  // Debug every 10 seconds
    Serial.printf("\n=== generateNmeaFromDataStore() called (dataStore size: %d) ===\n", dataStore.size());
  }

  // Helper to safely get path value
  auto getPath = [](const String& path, double& out) -> bool {
    auto it = dataStore.find(path);
    if (it != dataStore.end()) {
      if (it->second.isNumeric) {
        out = it->second.numValue;
        return true;
      } else if (!it->second.strValue.isEmpty()) {
        out = it->second.strValue.toDouble();
        return true;
      }
    }
    return false;
  };

  // Generate position sentences (GGA, GLL) if we have navigation.position
  auto posIt = dataStore.find("navigation.position");
  if (posIt != dataStore.end() && !posIt->second.jsonValue.isEmpty()) {
    // Parse the position JSON
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, posIt->second.jsonValue);
    if (!error && doc.containsKey("latitude") && doc.containsKey("longitude")) {
      double lat = doc["latitude"];
      double lon = doc["longitude"];

      // Generate GGA
      double altitude = 0;
      int satellites = 0;
      getPath("navigation.gnss.altitude", altitude);
      auto satIt = dataStore.find("navigation.gnss.satellitesInView");
      if (satIt != dataStore.end()) {
        satellites = satIt->second.isNumeric ? (int)satIt->second.numValue : (int)satIt->second.strValue.toDouble();
      }

      String gga = convertToGGA(lat, lon, iso8601Now(), satellites, altitude);
      if (debugCounter % 10 == 0) Serial.printf("  Generated GGA: %s", gga.c_str());
      broadcastNMEA0183(gga);

      // Generate GLL
      String gll = convertToGLL(lat, lon, iso8601Now());
      if (debugCounter % 10 == 0) Serial.printf("  Generated GLL: %s", gll.c_str());
      broadcastNMEA0183(gll);

      // Generate RMC if we have COG and SOG
      double cog, sog;
      if (getPath("navigation.courseOverGroundTrue", cog) && getPath("navigation.speedOverGround", sog)) {
        String rmc = convertToRMC(lat, lon, cog, sog, iso8601Now());
        if (debugCounter % 10 == 0) Serial.printf("  Generated RMC: %s", rmc.c_str());
        broadcastNMEA0183(rmc);
      }
    }
  }

  // Generate VTG if we have COG and SOG
  double cog, sog;
  if (getPath("navigation.courseOverGroundTrue", cog) && getPath("navigation.speedOverGround", sog)) {
    String vtg = convertToVTG(cog, sog);
    broadcastNMEA0183(vtg);
  }

  // Generate MWV (wind) sentences
  double windSpeed, windAngle;
  if (getPath("environment.wind.speedApparent", windSpeed) && getPath("environment.wind.angleApparent", windAngle)) {
    String mwv = convertToMWV(windAngle, windSpeed, 'R');  // Apparent = Relative
    broadcastNMEA0183(mwv);
  }
  if (getPath("environment.wind.speedTrue", windSpeed) && getPath("environment.wind.angleTrueWater", windAngle)) {
    String mwv = convertToMWV(windAngle, windSpeed, 'T');  // True
    broadcastNMEA0183(mwv);
  }

  // Generate DPT (depth)
  double depth;
  if (getPath("environment.depth.belowTransducer", depth)) {
    String dpt = convertToDPT(depth, 0.0);
    broadcastNMEA0183(dpt);
  }

  // Generate MTW (water temperature)
  double waterTemp;
  if (getPath("environment.water.temperature", waterTemp)) {
    String mtw = convertToMTW(waterTemp);
    broadcastNMEA0183(mtw);
  }
}

void loop() {
  // Process WiFiManager (non-blocking)
  wm.process();

  // Check WiFi connection and reconnect if needed
  uint32_t now = millis();

  // Update LED status indicators based on WiFi connection
  bool isConnectedToInternet = (WiFi.status() == WL_CONNECTED);
  updateLEDs(isConnectedToInternet);
  if (now - lastWifiCheck > 5000) { // Check every 5 seconds
    lastWifiCheck = now;
    bool isConnected = (WiFi.status() == WL_CONNECTED);

    if (!isConnected && wasWifiConnected) {
      // Connection was lost
      Serial.println("\n!!! WiFi connection lost !!!");
      Serial.println("Attempting to reconnect...");
      wasWifiConnected = false;
      wifiReconnectAttempts = 0;
    }

    if (!isConnected) {
      // Try to reconnect with exponential backoff
      if (now - lastWifiReconnect > 10000) { // Try every 10 seconds
        lastWifiReconnect = now;
        wifiReconnectAttempts++;

        if (wifiReconnectAttempts > 6) {
          // After 6 failed attempts (~1 minute), force disconnect and reset
          Serial.println("Too many reconnect failures - forcing reset");
          WiFi.disconnect(true);
          delay(2000);
          wifiReconnectAttempts = 0;
        } else {
          Serial.printf("Reconnect attempt %d/6\n", wifiReconnectAttempts);
          WiFi.reconnect();
        }
      }
    } else if (!wasWifiConnected) {
      // Just reconnected
      Serial.println("\n*** WiFi reconnected successfully ***");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      wasWifiConnected = true;
      wifiReconnectAttempts = 0;
    } else {
      wasWifiConnected = true;
    }
  }

  // Log WiFi status periodically for debugging
  if (now - lastStatusLog > 60000) { // Every 60 seconds
    lastStatusLog = now;
    Serial.println("\n=== Status Update ===");
    Serial.print("AP Status: ");
    Serial.println(WiFi.softAPgetStationNum() > 0 ? "Clients connected" : "No clients");
    Serial.print("WiFi Client: ");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to " + WiFi.SSID());
      Serial.println("IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("Not connected");
    }
    Serial.println("====================\n");
  }

  // Read NMEA sentences from Serial1 (RS485)
  static unsigned long lastRS485Activity = 0;
  static unsigned long lastRS485Report = 0;
  static int rs485BytesReceived = 0;

  while (Serial1.available()) {
    char c = Serial1.read();
    lastRS485Activity = now;
    rs485BytesReceived++;

      if (c == '\n' || c == '\r') {
        if (nmeaBuffer.length() > 6 && nmeaBuffer[0] == '$') {
          Serial.printf("RS485 RX: %s\n", nmeaBuffer.c_str());
          handleNmeaSentence(nmeaBuffer, "RS485");
        } else if (nmeaBuffer.length() > 0) {
          // Debug: Show what we received even if it's not valid NMEA
          Serial.printf("RS485 Invalid: [%s] (len=%d)\n", nmeaBuffer.c_str(), nmeaBuffer.length());
        }
        nmeaBuffer = "";
    } else if (c >= 32 && c <= 126) {
      if (nmeaBuffer.length() < 120) {
        nmeaBuffer += c;
      }
    } else {
      // Debug: Show ALL non-printable characters with their hex value
      Serial.printf("RS485: Non-printable 0x%02X ('%c')\n", (uint8_t)c, (c >= 32 && c <= 126) ? c : '?');
    }
  }

  // Report RS485 activity status every 10 seconds
  if (now - lastRS485Report >= 10000) {
    lastRS485Report = now;
    if (rs485BytesReceived > 0) {
      Serial.printf("\n[RS485 Status] Received %d bytes in last 10s\n", rs485BytesReceived);
      rs485BytesReceived = 0;
    } else if (now - lastRS485Activity > 30000) {
      // No activity for 30 seconds
      Serial.println("\n[RS485 Status] ⚠️ NO DATA for 30+ seconds");
      Serial.println("  Check: Wiring, Baud rate, Depth sounder power");
    }
  }

  // Read GPS data from SoftwareSerial
  // NOTE: GPS now uses SoftwareSerial (UART2 reserved for Single-Ended NMEA)
  static unsigned long lastGPSActivity = 0;
  static unsigned long lastGPSReport = 0;
  static int gpsBytesReceived = 0;

  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    lastGPSActivity = now;
    gpsBytesReceived++;

      if (c == '\n' || c == '\r') {
        if (gpsBuffer.length() > 6 && gpsBuffer[0] == '$') {
          Serial.printf("GPS RX: %s\n", gpsBuffer.c_str());
          handleNmeaSentence(gpsBuffer, nullptr);  // GPS modules send NMEA 0183
        }
        gpsBuffer = "";
    } else if (c >= 32 && c <= 126) {
      if (gpsBuffer.length() < 120) {
        gpsBuffer += c;
      }
    }
  }

  // Report GPS activity status every 10 seconds
  if (now - lastGPSReport >= 10000) {
    lastGPSReport = now;
    if (gpsBytesReceived > 0) {
      Serial.printf("\n[GPS] Received %d bytes in last 10s\n", gpsBytesReceived);
      gpsBytesReceived = 0;
    } else if (now - lastGPSActivity > 30000) {
      Serial.println("\n[GPS] ⚠️ NO DATA for 30+ seconds");
      Serial.println("  Check: GPS wiring (TX→GPIO25), baud rate (9600), satellite fix");
    }
  }

  // Read Single-Ended NMEA data (Direct NMEA 0183 - not RS485)
  #ifdef USE_SINGLEENDED_NMEA
    static unsigned long lastSingleEndedActivity = 0;
    static unsigned long lastSingleEndedReport = 0;
    static int singleEndedBytesReceived = 0;
    static bool debugRawBytes = true;  // Set to true to debug inverted signal issues

    while (SingleEndedSerial.available()) {
      char c = SingleEndedSerial.read();
      lastSingleEndedActivity = now;
      singleEndedBytesReceived++;

      // Hardware UART handles inversion automatically - no software processing needed!

      // Debug: Print first 20 raw bytes to verify correct reception
      if (debugRawBytes && singleEndedBytesReceived <= 20) {
        Serial.printf("Raw byte: 0x%02X ('%c') [hw-inverted]\n", (unsigned char)c, (c >= 32 && c <= 126) ? c : '.');
      }

      if (c == '\n' || c == '\r') {
        if (singleEndedBuffer.length() > 6 && singleEndedBuffer[0] == '$') {
          Serial.printf("Single-Ended RX: %s\n", singleEndedBuffer.c_str());
          handleNmeaSentence(singleEndedBuffer, "SingleEnded");
        }
        singleEndedBuffer = "";
      } else if (c >= 32 && c <= 126) {
        if (singleEndedBuffer.length() < 120) {
          singleEndedBuffer += c;
        }
      }
    }

    // Report Single-Ended NMEA activity status every 10 seconds
    if (now - lastSingleEndedReport >= 10000) {
      lastSingleEndedReport = now;
      if (singleEndedBytesReceived > 0) {
        Serial.printf("\n[Single-Ended NMEA] Received %d bytes in last 10s\n", singleEndedBytesReceived);
        singleEndedBytesReceived = 0;
      } else if (now - lastSingleEndedActivity > 30000) {
        Serial.println("\n[Single-Ended NMEA] ⚠️ NO DATA for 30+ seconds");
        Serial.println("  Check: Voltage divider/optocoupler wiring, device power, baud rate");
        Serial.println("  If using optocoupler: signal may be inverted (need hardware inverter)");
      }
    }
  #endif

  // Process NMEA2000 CAN messages
  if (n2kEnabled) {
    NMEA2000.ParseMessages();
  }

  // Read I2C sensors
  readI2CSensors();

  // Process Seatalk 1 data (if enabled)
  #ifdef USE_SEATALK1
    if (isSeatalk1Enabled()) {
      processSeatalk1();
    }
  #endif

  // Flush anchor persistence (ensures NVS writes happen on main task)
  flushAnchorPersist();

  // Process NMEA 0183 TCP Server (port 10110)
  processNMEA0183Server();

  // Generate and broadcast NMEA 0183 sentences from SignalK dataStore
  // This ensures per-path priority filtering
  static uint32_t lastNmeaGeneration = 0;
  if (now - lastNmeaGeneration > 1000) {  // Generate every 1 second
    lastNmeaGeneration = now;
    generateNmeaFromDataStore();
  }

  // TCP client connection and data processing
  connectToTcpServer();
  processTcpData();
  processDynDnsService();

  // Broadcast WebSocket deltas
  if (ws.count() > 0) {
    broadcastDeltas();
  }

  // Send WebSocket heartbeat to keep connections alive (every 20 seconds)
  if (now - lastWsPing > 20000) {
    lastWsPing = now;
    if (ws.count() > 0) {
      // Send a lightweight heartbeat delta to all clients
      DynamicJsonDocument heartbeat(256);
      heartbeat["context"] = "vessels." + vesselUUID;
      JsonArray updates = heartbeat.createNestedArray("updates");
      JsonObject update = updates.createNestedObject();
      update["timestamp"] = iso8601Now();
      JsonObject source = update.createNestedObject("source");
      source["label"] = serverName;
      source["type"] = "NMEA2000";
      JsonArray values = update.createNestedArray("values");
      JsonObject val = values.createNestedObject();
      val["path"] = "navigation.heartbeat";
      val["value"] = millis();

      String heartbeatMsg;
      serializeJson(heartbeat, heartbeatMsg);
      ws.textAll(heartbeatMsg);
    }
  }

  // Cleanup WebSocket clients periodically
  if (now - lastWsCleanup > WS_CLEANUP_MS) {
    lastWsCleanup = now;
    ws.cleanupClients();
  }

  // Cleanup web sessions periodically
  if (now - lastSessionCleanup > WEB_SESSION_CLEANUP_MS) {
    lastSessionCleanup = now;
    cleanupWebSessions();
  }

  delay(1);
}
