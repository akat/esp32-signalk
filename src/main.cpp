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

// Central handler to keep NMEA inputs consistent across all sources
void handleNmeaSentence(const String& sentence, const char* sourceTag) {
  if (sentence.length() < 7 || sentence[0] != '$') {
    return;
  }

  if (sourceTag != nullptr) {
    Serial.printf("NMEA [%s]: %s\n", sourceTag, sentence.c_str());
  }

  parseNMEASentence(sentence);

  // Re-broadcast to TCP clients so external tools see the same stream
  String broadcastSentence = sentence;
  broadcastSentence += "\r\n";
  broadcastNMEA0183(broadcastSentence);
}

// Serial port buffers
String gpsBuffer = "";
String nmeaBuffer = "";

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

  // GPS Module (Serial2)
  Serial.println("Starting GPS module...");
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS UART started on pins RX:" + String(GPS_RX) + " TX:" + String(GPS_TX));

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
uint32_t lastStatusLog = 0;
uint32_t lastWifiCheck = 0;
uint32_t lastWifiReconnect = 0;
uint32_t lastSessionCleanup = 0;
bool wasWifiConnected = false;
int wifiReconnectAttempts = 0;

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

  // Read GPS data from Serial2
  while (Serial2.available()) {
    char c = Serial2.read();

      if (c == '\n' || c == '\r') {
        if (gpsBuffer.length() > 6 && gpsBuffer[0] == '$') {
          handleNmeaSentence(gpsBuffer, nullptr);  // GPS modules send NMEA 0183
        }
        gpsBuffer = "";
    } else if (c >= 32 && c <= 126) {
      if (gpsBuffer.length() < 120) {
        gpsBuffer += c;
      }
    }
  }

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

  // TCP client connection and data processing
  connectToTcpServer();
  processTcpData();
  processDynDnsService();

  // Broadcast WebSocket deltas
  if (ws.count() > 0) {
    broadcastDeltas();
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
