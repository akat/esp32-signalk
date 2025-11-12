/*
  ESP32 SignalK Server - Full Implementation with Access Requests
  Compatible with SignalK clients and apps
  Features:
  - WiFiManager for easy setup (like SenseESP)
  - Full SignalK REST API structure
  - SignalK Security API with access requests
  - WebSocket with subscribe/unsubscribe
  - Authentication with tokens
  - NMEA0183 parsing (GPS)
  - Metadata & units support
  - Delta compression
*/

#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#include <time.h>
#include <algorithm>
#include <vector>
#include <map>
#include <set>

// ====== CONFIGURATION ======
#define NMEA_RX 16
#define NMEA_TX 17
#define NMEA_BAUD 4800

#define WS_DELTA_MIN_MS 500        // Minimum delta broadcast interval (500ms for faster updates)
#define WS_CLEANUP_MS 5000         // WebSocket cleanup interval

// TCP Client Configuration
#define TCP_RECONNECT_DELAY 5000   // Reconnect attempt interval (ms)
#define NMEA_CLIENT_TIMEOUT 60000  // Client inactivity timeout (ms) - increased to 60s
#define MAX_SENTENCES_PER_SECOND 100 // Rate limiting - increased from 50 to 100

// WiFi AP Configuration
// This AP is used both for direct connections and for the WiFiManager config portal
#define AP_SSID "ESP32-SignalK"    // WiFi network name
#define AP_PASSWORD "signalk123"   // WiFi password (min 8 chars)
#define AP_CHANNEL 1               // WiFi channel
#define AP_HIDDEN false            // Broadcast SSID
#define AP_MAX_CONNECTIONS 4       // Max simultaneous connections

// WiFi Client Configuration is now handled by WiFiManager
// Connect to the AP and navigate to http://192.168.4.1 to configure WiFi network

// ====== GLOBALS ======
AsyncWebServer server(3000);  // SignalK default port
AsyncWebSocket ws("/signalk/v1/stream");
Preferences prefs;
WiFiManager wm;  // WiFiManager instance for config portal

// WebSocket client authentication tracking
std::map<uint32_t, String> clientTokens;  // clientId -> token

// SignalK vessel UUID (persistent)
String vesselUUID;
String serverName = "ESP32-SignalK";

// TCP Client State Machine
enum TcpClientState {
  TCP_DISCONNECTED,
  TCP_CONNECTING,
  TCP_CONNECTED,
  TCP_ERROR
};

// TCP Client for external SignalK server
WiFiClient tcpClient;
String tcpServerHost = "";
int tcpServerPort = 10110;
bool tcpEnabled = false;
uint32_t lastTcpReconnect = 0;
uint32_t tcpConnectedTime = 0;
String tcpBuffer = "";
TcpClientState tcpState = TCP_DISCONNECTED;

// TCP Server for receiving NMEA data (for mocking)
WiFiServer nmeaTcpServer(10110);
WiFiClient nmeaTcpClient;
String nmeaTcpBuffer = "";
uint32_t nmeaLastActivity = 0;

// Rate limiting for NMEA sentences
uint32_t sentenceCount = 0;
uint32_t sentenceWindowStart = 0;

// ====== PATH STORAGE ======
struct PathValue {
  JsonVariant value;
  String strValue;      // For string storage
  double numValue;
  bool isNumeric = false;
  bool isJson = false;
  String jsonValue;
  String timestamp;     // ISO8601
  String source;        // e.g., "nmea0183.GPS"
  
  // Metadata
  String units;
  String description;
  
  bool changed;         // For delta compression
};

std::map<String, PathValue> dataStore;
std::map<String, PathValue> lastSentValues; // For delta compression

// ====== WEBSOCKET SUBSCRIPTIONS ======
struct ClientSubscription {
  std::set<String> paths;
  String format; // "delta" or "full"
};

std::map<uint32_t, ClientSubscription> clientSubscriptions; // clientId -> subscription

// ====== ACCESS REQUESTS & AUTHENTICATION ======
// Storage for access requests (for polling)
struct AccessRequestData {
  String requestId;       // Request ID (usually same as clientId)
  String clientId;        // Client identifier
  String description;     // Client description
  String permissions;     // Requested permissions (read/readwrite)
  String token;           // Generated token (if approved)
  String state;           // "PENDING", "COMPLETED", or "DENIED"
  String permission;      // "APPROVED" or "DENIED"
  unsigned long timestamp; // Request timestamp
};
std::map<String, AccessRequestData> accessRequests;

// Storage for approved tokens (persistent)
struct ApprovedToken {
  String token;
  String clientId;
  String description;
  String permissions;
  unsigned long approvedAt;
};
std::map<String, ApprovedToken> approvedTokens; // Key: token

// ====== EXPO PUSH NOTIFICATIONS ======
std::vector<String> expoTokens;  // List of Expo push tokens
unsigned long lastPushNotification = 0;  // Last notification timestamp
const unsigned long PUSH_NOTIFICATION_COOLDOWN = 10000;  // 10 seconds

// Expo token management
void saveExpoTokens() {
  prefs.begin("signalk", false);
  prefs.putInt("expoCount", expoTokens.size());
  for (size_t i = 0; i < expoTokens.size(); i++) {
    String key = "expo" + String(i);
    prefs.putString(key.c_str(), expoTokens[i]);
  }
  prefs.end();
  Serial.printf("Saved %d Expo tokens\n", expoTokens.size());
}

void loadExpoTokens() {
  prefs.begin("signalk", true);
  int count = prefs.getInt("expoCount", 0);
  expoTokens.clear();
  for (int i = 0; i < count; i++) {
    String key = "expo" + String(i);
    String token = prefs.getString(key.c_str(), "");
    if (token.length() > 0) {
      expoTokens.push_back(token);
    }
  }
  prefs.end();
  Serial.printf("Loaded %d Expo tokens\n", expoTokens.size());
}

bool addExpoToken(const String& token) {
  // Check if token already exists
  for (const auto& existingToken : expoTokens) {
    if (existingToken == token) {
      return false;  // Already exists
    }
  }
  expoTokens.push_back(token);
  saveExpoTokens();
  return true;
}

void sendExpoPushNotification(const String& title, const String& body, const String& alarmType = "", const String& data = "") {
  unsigned long now = millis();

  // Rate limiting check
  if (now - lastPushNotification < PUSH_NOTIFICATION_COOLDOWN) {
    Serial.println("Push notification rate limited");
    return;
  }

  if (expoTokens.empty()) {
    Serial.println("No Expo tokens registered");
    return;
  }

  lastPushNotification = now;

  // Send to each registered token
  for (const auto& token : expoTokens) {
    Serial.printf("Sending push notification to: %s\n", token.substring(0, 30).c_str());

    // Build JSON payload for Expo push API
    DynamicJsonDocument doc(512);
    doc["to"] = token;
    doc["title"] = title;
    doc["body"] = body;
    doc["priority"] = "high";

    // Set sound and channelId based on alarm type
    if (alarmType == "geofence") {
      doc["sound"] = "geofence_alarm.wav";
      doc["channelId"] = "geofence-alarms";
    } else if (alarmType == "depth") {
      doc["sound"] = "geofence_alarm.wav";
      doc["channelId"] = "geofence-alarms";
    } else if (alarmType == "wind") {
      doc["sound"] = "geofence_alarm.wav";
      doc["channelId"] = "geofence-alarms";
    } else {
      doc["sound"] = "default";
    }

    if (data.length() > 0) {
      doc["data"] = data;
    }

    String payload;
    serializeJson(doc, payload);

    // Send HTTP POST to Expo push API
    WiFiClientSecure client;
    client.setInsecure();  // Skip certificate validation for simplicity

    if (client.connect("exp.host", 443)) {
      client.println("POST /--/api/v2/push/send HTTP/1.1");
      client.println("Host: exp.host");
      client.println("Content-Type: application/json");
      client.println("Accept: application/json");
      client.print("Content-Length: ");
      client.println(payload.length());
      client.println("Connection: close");
      client.println();
      client.print(payload);

      // Wait for response
      unsigned long timeout = millis() + 5000;
      while (client.connected() && millis() < timeout) {
        if (client.available()) {
          String line = client.readStringUntil('\n');
          if (line.startsWith("HTTP/")) {
            Serial.printf("Push API response: %s\n", line.c_str());
          }
        }
      }
      client.stop();
    } else {
      Serial.println("Failed to connect to Expo push API");
    }
  }
}

// Token management functions
void saveApprovedTokens() {
  prefs.begin("signalk", false);
  prefs.putInt("tokenCount", approvedTokens.size());

  int index = 0;
  for (auto& pair : approvedTokens) {
    String prefix = "tok" + String(index) + "_";
    prefs.putString((prefix + "token").c_str(), pair.second.token);
    prefs.putString((prefix + "clientId").c_str(), pair.second.clientId);
    prefs.putString((prefix + "desc").c_str(), pair.second.description);
    prefs.putString((prefix + "perms").c_str(), pair.second.permissions);
    prefs.putULong((prefix + "time").c_str(), pair.second.approvedAt);
    index++;
  }

  prefs.end();
  Serial.println("Approved tokens saved to flash");
}

void loadApprovedTokens() {
  prefs.begin("signalk", true);
  int tokenCount = prefs.getInt("tokenCount", 0);

  Serial.printf("Loading %d approved tokens from flash...\n", tokenCount);

  for (int i = 0; i < tokenCount; i++) {
    String prefix = "tok" + String(i) + "_";
    ApprovedToken token;
    token.token = prefs.getString((prefix + "token").c_str(), "");
    token.clientId = prefs.getString((prefix + "clientId").c_str(), "");
    token.description = prefs.getString((prefix + "desc").c_str(), "");
    token.permissions = prefs.getString((prefix + "perms").c_str(), "read");
    token.approvedAt = prefs.getULong((prefix + "time").c_str(), 0);

    if (token.token.length() > 0) {
      approvedTokens[token.token] = token;
      Serial.printf("  Loaded token for: %s\n", token.clientId.c_str());
    }
  }

  prefs.end();
}

bool isTokenValid(const String& token) {
  return approvedTokens.find(token) != approvedTokens.end();
}

String extractBearerToken(AsyncWebServerRequest* request) {
  if (!request->hasHeader("Authorization")) {
    return "";
  }

  String authHeader = request->getHeader("Authorization")->value();
  if (authHeader.startsWith("Bearer ")) {
    return authHeader.substring(7); // Remove "Bearer " prefix
  }

  return "";
}

// ====== NMEA STATE ======
struct GPSData {
  double lat = NAN;
  double lon = NAN;
  double sog = NAN;  // m/s
  double cog = NAN;  // radians
  double heading = NAN; // radians
  double altitude = NAN; // meters
  int satellites = 0;
  String fixQuality;
  String timestamp;
} gpsData;

// ====== GEOFENCE & ALARMS ======
// Geofence configuration
struct GeofenceConfig {
  bool enabled = false;
  double anchorLat = NAN;
  double anchorLon = NAN;
  double radius = 100.0;  // meters
  uint32_t anchorTimestamp = 0;
  bool alarmActive = false;
  double lastDistance = NAN;
} geofence;

// Depth alarm configuration
struct DepthAlarmConfig {
  bool enabled = false;
  double threshold = 2.0;  // meters
  bool alarmActive = false;
  double lastDepth = NAN;
  uint32_t lastSampleTime = 0;
} depthAlarm;

// Wind alarm configuration
struct WindAlarmConfig {
  bool enabled = false;
  double threshold = 20.0;  // knots
  bool alarmActive = false;
  double lastWind = NAN;
  uint32_t lastSampleTime = 0;
} windAlarm;

// SignalK Notifications storage
std::map<String, String> notifications; // path -> state ("alarm", "warn", "alert", "normal")

// ====== UTILITY FUNCTIONS ======
String generateUUID() {
  char uuid[57];
  snprintf(uuid, sizeof(uuid), "urn:mrn:signalk:uuid:%08x-%04x-%04x-%04x-%012x",
    esp_random(), esp_random() & 0xFFFF, esp_random() & 0xFFFF,
    esp_random() & 0xFFFF, esp_random());
  return String(uuid);
}

String iso8601Now() {
  time_t now = time(nullptr);
  if (now < 100000) return ""; // Time not set
  struct tm t;
  gmtime_r(&now, &t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &t);
  return String(buf);
}

double knotsToMS(double knots) { return knots * 0.514444; }
double msToKnots(double ms) { return ms / 0.514444; }
double degToRad(double deg) { return deg * M_PI / 180.0; }

// Haversine distance calculation (returns meters)
double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0; // Earth radius in meters
  const double toRad = M_PI / 180.0;

  double dLat = (lat2 - lat1) * toRad;
  double dLon = (lon2 - lon1) * toRad;
  double a = sin(dLat/2) * sin(dLat/2) +
             cos(lat1 * toRad) * cos(lat2 * toRad) *
             sin(dLon/2) * sin(dLon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

// Forward declarations
void updateGeofence();
void updateDepthAlarm(double depth);
void updateWindAlarm(double windSpeedMS);

// ====== PATH OPERATIONS ======
void setPathValue(const String& path, double value, const String& source = "nmea0183.GPS",
                  const String& units = "", const String& description = "") {
  PathValue& pv = dataStore[path];
  pv.numValue = value;
  pv.isNumeric = true;
  pv.isJson = false;
  pv.jsonValue = "";
  pv.timestamp = iso8601Now();
  pv.source = source;
  pv.units = units;
  pv.description = description;
  pv.changed = true;

  // Always mark as changed - let the WebSocket throttling handle update frequency
  // This ensures we don't miss important navigation updates
}

void setPathValue(const String& path, const String& value, const String& source = "nmea0183.GPS",
                  const String& units = "", const String& description = "") {
  PathValue& pv = dataStore[path];
  pv.strValue = value;
  pv.isNumeric = false;
  pv.isJson = false;
  pv.jsonValue = "";
  pv.timestamp = iso8601Now();
  pv.source = source;
  pv.units = units;
  pv.description = description;
  pv.changed = true;

  // Always mark as changed - let the WebSocket throttling handle update frequency
}

void setPathValueJson(const String& path, const String& jsonValue, const String& source = "nmea0183.GPS",
                      const String& units = "", const String& description = "") {
  PathValue& pv = dataStore[path];
  pv.isNumeric = false;
  pv.isJson = true;
  pv.jsonValue = jsonValue;
  pv.timestamp = iso8601Now();
  pv.source = source;
  pv.units = units;
  pv.description = description;
  pv.changed = true;

  // Persist important configuration paths to flash
  if (path == "navigation.anchor.akat") {
    Serial.printf("Attempting to persist %d bytes to flash\n", jsonValue.length());

    if (jsonValue.length() > 1900) {
      Serial.println("WARNING: JSON too large for NVS, truncating may occur");
    }

    prefs.begin("signalk", false);
    size_t written = prefs.putString("anchor.akat", jsonValue);

    // Verify it was written
    String readBack = prefs.getString("anchor.akat", "");
    prefs.end();

    if (written > 0 && readBack == jsonValue) {
      Serial.printf("SUCCESS: Persisted %d bytes to flash and verified\n", written);
    } else {
      Serial.printf("ERROR: Failed to persist (wrote %d bytes, readback length %d)\n", written, readBack.length());
    }
  }

  // Always mark as changed - let the WebSocket throttling handle update frequency
}

void updateNavigationPosition(double lat, double lon, const String& source = "nmea0183.GPS") {
  if (isnan(lat) || isnan(lon)) {
    Serial.println("Position update rejected - NaN values");
    return;
  }

  DynamicJsonDocument doc(128);
  doc["latitude"] = lat;
  doc["longitude"] = lon;

  String json;
  serializeJson(doc, json);

  // Serial.println("=== Position Update ===");
  // Serial.printf("Lat: %.6f, Lon: %.6f\n", lat, lon);
  // Serial.println("JSON: " + json);
  // Serial.println("======================");

  setPathValueJson("navigation.position", json, source, "", "Vessel position");

  // Trigger geofence monitoring
  updateGeofence();
}

// ====== SIGNALK NOTIFICATIONS ======
void setNotification(const String& path, const String& state, const String& message) {
  notifications[path] = state;

  // Build notification object
  DynamicJsonDocument doc(512);
  doc["state"] = state;
  doc["method"] = "visual";
  doc["timestamp"] = iso8601Now();
  doc["message"] = message;

  String json;
  serializeJson(doc, json);

  // Store in dataStore with special prefix
  setPathValueJson("notifications." + path, json, "esp32.alarms", "", "Alarm notification");

  Serial.printf("Notification: %s -> %s: %s\n", path.c_str(), state.c_str(), message.c_str());
}

void clearNotification(const String& path) {
  setNotification(path, "normal", "");
}

// ====== GEOFENCE MONITORING ======
void updateGeofence() {
  if (!geofence.enabled) {
    if (geofence.alarmActive) {
      clearNotification("geofence.exit");
      geofence.alarmActive = false;
    }
    return;
  }

  // Need valid anchor and current position
  if (isnan(geofence.anchorLat) || isnan(geofence.anchorLon)) {
    return;
  }
  if (isnan(gpsData.lat) || isnan(gpsData.lon)) {
    return;
  }

  // Calculate distance
  double distance = haversineDistance(gpsData.lat, gpsData.lon,
                                       geofence.anchorLat, geofence.anchorLon);
  geofence.lastDistance = distance;

  // Check if outside geofence
  bool outside = distance > geofence.radius;

  if (outside) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Vessel left geofence: %.0f m (> %.0f m)", distance, geofence.radius);

    if (!geofence.alarmActive) {
      // First time triggering alarm
      setNotification("geofence.exit", "alarm", String(msg));
      geofence.alarmActive = true;
      Serial.printf("GEOFENCE ALARM: %s\n", msg);
    }

    // Send push notification continuously while outside (respects rate limit)
    sendExpoPushNotification("Geofence Alert", String(msg), "geofence");
  } else if (!outside && geofence.alarmActive) {
    // Clear alarm
    clearNotification("geofence.exit");
    geofence.alarmActive = false;
    Serial.println("Geofence: Back inside");
  }

  // Update status in dataStore for dashboard
  DynamicJsonDocument statusDoc(256);
  statusDoc["enabled"] = geofence.enabled;
  statusDoc["radius"] = geofence.radius;
  statusDoc["distance"] = round(distance);
  statusDoc["inside"] = !outside;
  if (!isnan(geofence.anchorLat)) {
    statusDoc["anchor"]["lat"] = geofence.anchorLat;
    statusDoc["anchor"]["lon"] = geofence.anchorLon;
  }
  String statusJson;
  serializeJson(statusDoc, statusJson);
  setPathValueJson("navigation.anchor.status", statusJson, "esp32.geofence", "", "Geofence status");
}

// ====== DEPTH ALARM MONITORING ======
void updateDepthAlarm(double depth) {
  depthAlarm.lastDepth = depth;
  depthAlarm.lastSampleTime = millis();

  if (!depthAlarm.enabled) {
    if (depthAlarm.alarmActive) {
      clearNotification("depth.alarm");
      depthAlarm.alarmActive = false;
    }
    return;
  }

  if (isnan(depth)) {
    return;
  }

  bool triggered = depth <= depthAlarm.threshold;

  if (triggered && !depthAlarm.alarmActive) {
    // Trigger alarm
    char msg[128];
    snprintf(msg, sizeof(msg), "Depth %.1f m (limit %.1f m)", depth, depthAlarm.threshold);
    setNotification("depth.alarm", "alarm", String(msg));
    depthAlarm.alarmActive = true;
    Serial.printf("DEPTH ALARM: %s\n", msg);

    // Send push notification
    sendExpoPushNotification("Depth Alert", String(msg), "depth");
  } else if (!triggered && depthAlarm.alarmActive) {
    // Clear alarm
    clearNotification("depth.alarm");
    depthAlarm.alarmActive = false;
    Serial.println("Depth: Back to normal");
  }
}

// ====== WIND ALARM MONITORING ======
void updateWindAlarm(double windSpeedMS) {
  const double RESET_HYSTERESIS_KNOTS = 1.0;

  // Convert m/s to knots
  double windKnots = msToKnots(windSpeedMS);
  windAlarm.lastWind = windKnots;
  windAlarm.lastSampleTime = millis();

  if (!windAlarm.enabled) {
    if (windAlarm.alarmActive) {
      clearNotification("wind.alarm");
      windAlarm.alarmActive = false;
    }
    return;
  }

  if (isnan(windSpeedMS)) {
    return;
  }

  bool triggered = windKnots >= windAlarm.threshold;

  if (triggered && !windAlarm.alarmActive) {
    // Trigger alarm
    char msg[128];
    snprintf(msg, sizeof(msg), "True wind %.1f kn (limit %.1f kn)", windKnots, windAlarm.threshold);
    setNotification("wind.alarm", "alarm", String(msg));
    windAlarm.alarmActive = true;
    Serial.printf("WIND ALARM: %s\n", msg);

    // Send push notification
    sendExpoPushNotification("Wind Alert", String(msg), "wind");
  } else if (windAlarm.alarmActive && windKnots <= (windAlarm.threshold - RESET_HYSTERESIS_KNOTS)) {
    // Clear alarm with hysteresis
    clearNotification("wind.alarm");
    windAlarm.alarmActive = false;
    Serial.println("Wind: Back below threshold");
  }
}

// ====== NMEA PARSING ======
bool validateNmeaChecksum(const String& sentence) {
  int starPos = sentence.indexOf('*');
  if (starPos < 0) return true;  // No checksum present, accept sentence

  String data = sentence.substring(1, starPos);  // Skip '$'
  String checksumStr = sentence.substring(starPos + 1);

  if (checksumStr.length() != 2) return false;  // Invalid checksum format

  uint8_t checksum = 0;
  for (int i = 0; i < data.length(); i++) {
    checksum ^= data[i];
  }

  uint8_t expected = strtol(checksumStr.c_str(), NULL, 16);
  return checksum == expected;
}

double nmeaCoordToDec(const String& coord, const String& hemisphere) {
  if (coord.length() < 4) {
    Serial.println("Coord too short: " + coord);
    return NAN;
  }

  int dotPos = coord.indexOf('.');
  if (dotPos < 0) {
    Serial.println("No dot in coord: " + coord);
    return NAN;
  }

  // NMEA format: latitude is DDMM.MMMM (2 digits degrees), longitude is DDDMM.MMMM (3 digits degrees)
  // Determine if this is lat or lon based on the dot position
  // Latitude: dot should be at position 4 (DDMM.M)
  // Longitude: dot should be at position 5 (DDDMM.M)
  int degLen = (dotPos == 4) ? 2 : 3;

  String degStr = coord.substring(0, degLen);
  String minStr = coord.substring(degLen);

  double degrees = degStr.toDouble();
  double minutes = minStr.toDouble();
  double decimal = degrees + (minutes / 60.0);

  // Serial.printf("Coord: %s, Hemisphere: %s\n", coord.c_str(), hemisphere.c_str());
  // Serial.printf("  Degrees: %s = %.2f, Minutes: %s = %.6f\n", degStr.c_str(), degrees, minStr.c_str(), minutes);
  // Serial.printf("  Result: %.6f\n", decimal);

  if (hemisphere == "S" || hemisphere == "W") {
    decimal = -decimal;
  }

  return decimal;
}

std::vector<String> splitNMEA(const String& sentence) {
  std::vector<String> fields;
  int start = 0;
  int idx;
  
  String s = sentence;
  int starPos = s.indexOf('*');
  if (starPos > 0) {
    s = s.substring(0, starPos);
  }
  
  while ((idx = s.indexOf(',', start)) >= 0) {
    fields.push_back(s.substring(start, idx));
    start = idx + 1;
  }
  fields.push_back(s.substring(start));
  
  return fields;
}

void parseNMEASentence(const String& sentence) {
  if (sentence.length() < 7 || sentence[0] != '$') return;

  // Validate checksum if present (only warn, don't reject for now)
  if (!validateNmeaChecksum(sentence)) {
    Serial.println("NMEA: Warning - checksum validation failed for: " + sentence);
    // Continue processing anyway - some devices send sentences without checksums
    // or with incorrect checksums but the data is still valid
  }

  std::vector<String> fields = splitNMEA(sentence);
  if (fields.size() < 3) return;
  
  String msgType = fields[0];
  
  // $GPRMC - Recommended Minimum
  if (msgType.endsWith("RMC") && fields.size() >= 10) {
    String status = fields[2];
    if (status != "A") return; // Not valid
    
    double lat = nmeaCoordToDec(fields[3], fields[4]);
    double lon = nmeaCoordToDec(fields[5], fields[6]);
    double sog = fields[7].toDouble(); // knots
    double cog = fields[8].toDouble(); // degrees
    
    if (!isnan(lat) && !isnan(lon)) {
      gpsData.lat = lat;
      gpsData.lon = lon;
      gpsData.timestamp = iso8601Now();

      // Only use the combined position object, not separate lat/lon paths
      updateNavigationPosition(lat, lon);
    }
    
    if (!isnan(sog) && sog >= 0) {
      gpsData.sog = knotsToMS(sog);
      setPathValue("navigation.speedOverGround", gpsData.sog, "nmea0183.GPS", "m/s", "Speed over ground");
    }
    
    if (!isnan(cog) && cog >= 0 && cog <= 360) {
      gpsData.cog = degToRad(cog);
      setPathValue("navigation.courseOverGroundTrue", gpsData.cog, "nmea0183.GPS", "rad", "Course over ground (true)");
    }
  }
  
  // $GPGGA - GPS Fix Data
  else if (msgType.endsWith("GGA") && fields.size() >= 15) {
    double lat = nmeaCoordToDec(fields[2], fields[3]);
    double lon = nmeaCoordToDec(fields[4], fields[5]);
    String quality = fields[6];
    int sats = fields[7].toInt();
    double alt = fields[9].toDouble();
    
    if (!isnan(lat) && !isnan(lon)) {
      gpsData.lat = lat;
      gpsData.lon = lon;
      gpsData.satellites = sats;
      gpsData.fixQuality = quality;
      gpsData.timestamp = iso8601Now();

      // Only use the combined position object, not separate lat/lon paths
      setPathValue("navigation.gnss.satellitesInView", (double)sats, "nmea0183.GPS", "", "Satellites in view");
      updateNavigationPosition(lat, lon);
    }
    
    if (!isnan(alt)) {
      gpsData.altitude = alt;
      setPathValue("navigation.gnss.altitude", alt, "nmea0183.GPS", "m", "Altitude");
    }
  }
  
  // $GPVTG - Track & Speed
  else if (msgType.endsWith("VTG") && fields.size() >= 9) {
    double cog = fields[1].toDouble();
    double sog = fields[5].toDouble(); // knots
    
    if (!isnan(cog) && cog >= 0 && cog <= 360) {
      gpsData.cog = degToRad(cog);
      setPathValue("navigation.courseOverGroundTrue", gpsData.cog, "nmea0183.GPS", "rad", "Course over ground");
    }
    
    if (!isnan(sog) && sog >= 0) {
      gpsData.sog = knotsToMS(sog);
      setPathValue("navigation.speedOverGround", gpsData.sog, "nmea0183.GPS", "m/s", "Speed over ground");
    }
  }
  
  // $HCHDG or $GPHDG - Heading
  else if (msgType.endsWith("HDG") && fields.size() >= 2) {
    double heading = fields[1].toDouble();
    if (!isnan(heading) && heading >= 0 && heading <= 360) {
      gpsData.heading = degToRad(heading);
      setPathValue("navigation.headingMagnetic", gpsData.heading, "nmea0183.GPS", "rad", "Heading (magnetic)");
    }
  }

  // $GPGLL - Geographic Position - Latitude/Longitude
  else if (msgType.endsWith("GLL") && fields.size() >= 7) {
    String status = fields[6];
    if (status != "A") return; // Not valid

    double lat = nmeaCoordToDec(fields[1], fields[2]);
    double lon = nmeaCoordToDec(fields[3], fields[4]);

    if (!isnan(lat) && !isnan(lon)) {
      gpsData.lat = lat;
      gpsData.lon = lon;
      gpsData.timestamp = iso8601Now();

      // Only use the combined position object, not separate lat/lon paths
      updateNavigationPosition(lat, lon);
    }
  }

  // $IIHDM - Heading Magnetic
  else if (msgType.endsWith("HDM") && fields.size() >= 2) {
    double heading = fields[1].toDouble();
    if (!isnan(heading) && heading >= 0 && heading <= 360) {
      setPathValue("navigation.headingMagnetic", degToRad(heading), "nmea0183.GPS", "rad", "Heading (magnetic)");
    }
  }

  // $IIHDT - Heading True
  else if (msgType.endsWith("HDT") && fields.size() >= 2) {
    double heading = fields[1].toDouble();
    if (!isnan(heading) && heading >= 0 && heading <= 360) {
      setPathValue("navigation.headingTrue", degToRad(heading), "nmea0183.GPS", "rad", "Heading (true)");
    }
  }

  // $IIMWD - Meteorological Composite
  else if (msgType.endsWith("MWD") && fields.size() >= 8) {
    double windDirTrue = fields[1].toDouble();
    double windDirMag = fields[3].toDouble();
    double windSpeedKnots = fields[5].toDouble();
    double windSpeedMs = knotsToMS(windSpeedKnots);

    if (!isnan(windDirTrue) && windDirTrue >= 0 && windDirTrue <= 360) {
      setPathValue("environment.wind.directionTrue", degToRad(windDirTrue), "nmea0183.GPS", "rad", "Wind direction (true)");
    }
    if (!isnan(windDirMag) && windDirMag >= 0 && windDirMag <= 360) {
      setPathValue("environment.wind.directionMagnetic", degToRad(windDirMag), "nmea0183.GPS", "rad", "Wind direction (magnetic)");
    }
    if (!isnan(windSpeedMs) && windSpeedMs >= 0) {
      setPathValue("environment.wind.speedTrue", windSpeedMs, "nmea0183.GPS", "m/s", "Wind speed (true)");
      // Trigger wind alarm monitoring
      updateWindAlarm(windSpeedMs);
    }
  }

  // $IIVDR - Set and Drift
  else if (msgType.endsWith("VDR") && fields.size() >= 6) {
    double set = fields[1].toDouble();
    double drift = fields[3].toDouble();

    if (!isnan(set) && set >= 0 && set <= 360) {
      setPathValue("navigation.current.setTrue", degToRad(set), "nmea0183.GPS", "rad", "Current set (true)");
    }
    if (!isnan(drift) && drift >= 0) {
      setPathValue("navigation.current.drift", knotsToMS(drift), "nmea0183.GPS", "m/s", "Current drift");
    }
  }

  // $IIVHW - Water speed and heading
  else if (msgType.endsWith("VHW") && fields.size() >= 8) {
    double headingTrue = fields[1].toDouble();
    double headingMag = fields[3].toDouble();
    double speedKnots = fields[5].toDouble();
    double speedMs = knotsToMS(speedKnots);

    if (!isnan(headingTrue) && headingTrue >= 0 && headingTrue <= 360) {
      setPathValue("navigation.headingTrue", degToRad(headingTrue), "nmea0183.GPS", "rad", "Heading (true)");
    }
    if (!isnan(headingMag) && headingMag >= 0 && headingMag <= 360) {
      setPathValue("navigation.headingMagnetic", degToRad(headingMag), "nmea0183.GPS", "rad", "Heading (magnetic)");
    }
    if (!isnan(speedMs) && speedMs >= 0) {
      setPathValue("navigation.speedThroughWater", speedMs, "nmea0183.GPS", "m/s", "Speed through water");
    }
  }

  // $IIVPW - Speed Parallel to Wind
  else if (msgType.endsWith("VPW") && fields.size() >= 3) {
    double speedKnots = fields[1].toDouble();
    double speedMs = knotsToMS(speedKnots);

    if (!isnan(speedMs) && speedMs >= 0) {
      setPathValue("navigation.speedThroughWater", speedMs, "nmea0183.GPS", "m/s", "Speed through water");
    }
  }

  // $IIMWV - Wind Speed and Angle
  else if (msgType.endsWith("MWV") && fields.size() >= 6) {
    double windAngle = fields[1].toDouble();
    String reference = fields[2];
    double windSpeedKnots = fields[3].toDouble();
    double windSpeedMs = knotsToMS(windSpeedKnots);
    String status = fields[5];

    if (status != "A") return; // Not valid

    if (!isnan(windAngle) && windAngle >= 0 && windAngle <= 360 && !isnan(windSpeedMs) && windSpeedMs >= 0) {
      if (reference == "R") {
        // Relative wind
        setPathValue("environment.wind.angleApparent", degToRad(windAngle), "nmea0183.GPS", "rad", "Apparent wind angle");
        setPathValue("environment.wind.speedApparent", windSpeedMs, "nmea0183.GPS", "m/s", "Apparent wind speed");
      } else if (reference == "T") {
        // True wind
        setPathValue("environment.wind.angleTrueWater", degToRad(windAngle), "nmea0183.GPS", "rad", "True wind angle");
        setPathValue("environment.wind.speedTrue", windSpeedMs, "nmea0183.GPS", "m/s", "True wind speed");
        // Trigger wind alarm monitoring
        updateWindAlarm(windSpeedMs);
      }
    }
  }

  // $IIVWT - Wind Speed and Angle True
  else if (msgType.endsWith("VWT") && fields.size() >= 7) {
    double windAngleL = fields[1].toDouble();
    String dirL = fields[2];
    double windAngleR = fields[3].toDouble();
    String dirR = fields[4];
    double windSpeedKnots = fields[5].toDouble();
    double windSpeedMs = knotsToMS(windSpeedKnots);

    if (!isnan(windSpeedMs) && windSpeedMs >= 0) {
      setPathValue("environment.wind.speedTrue", windSpeedMs, "nmea0183.GPS", "m/s", "True wind speed");
      // Trigger wind alarm monitoring
      updateWindAlarm(windSpeedMs);
    }

    // Use left wind angle if available, otherwise right
    double windAngle = !isnan(windAngleL) ? windAngleL : windAngleR;
    if (!isnan(windAngle) && windAngle >= 0 && windAngle <= 360) {
      setPathValue("environment.wind.angleTrueWater", degToRad(windAngle), "nmea0183.GPS", "rad", "True wind angle");
    }
  }

  // $GPWCV - Waypoint Closure Velocity
  else if (msgType.endsWith("WCV") && fields.size() >= 4) {
    double velocityKnots = fields[1].toDouble();
    double velocityMs = knotsToMS(velocityKnots);

    if (!isnan(velocityMs) && velocityMs >= 0) {
      setPathValue("navigation.course.nextPoint.velocityMadeGood", velocityMs, "nmea0183.GPS", "m/s", "Velocity made good to waypoint");
    }
  }

  // $GPXTE - Cross-Track Error
  else if (msgType.endsWith("XTE") && fields.size() >= 6) {
    String status1 = fields[1];
    String status2 = fields[2];
    double xteNm = fields[3].toDouble();
    String direction = fields[4];

    if (status1 == "A" && status2 == "A" && !isnan(xteNm)) {
      double xteM = xteNm * 1852.0; // Convert nautical miles to meters
      if (direction == "L") xteM = -xteM; // Left is negative
      setPathValue("navigation.course.crossTrackError", xteM, "nmea0183.GPS", "m", "Cross-track error");
    }
  }

  // $GPZDA - Time & Date
  else if (msgType.endsWith("ZDA") && fields.size() >= 5) {
    int hour = fields[1].toInt();
    int minute = fields[2].toInt();
    int second = fields[3].toInt();
    int day = fields[4].toInt();
    int month = fields[5].toInt();
    int year = fields[6].toInt();

    if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
      // Could set system time here if needed
      // For now, just acknowledge the time data
    }
  }

  // $IIDBT - Depth of Water
  else if (msgType.endsWith("DBT") && fields.size() >= 7) {
    double depthFeet = fields[1].toDouble();
    double depthMeters = fields[3].toDouble();
    double depthFathoms = fields[5].toDouble();

    // Use meters if available, otherwise convert from feet
    double depth = !isnan(depthMeters) ? depthMeters : (depthFeet * 0.3048);

    if (!isnan(depth) && depth >= 0) {
      setPathValue("environment.depth.belowTransducer", depth, "nmea0183.GPS", "m", "Depth below transducer");
      // Trigger depth alarm monitoring
      updateDepthAlarm(depth);
    }
  }

  // $GPGSV - GPS Satellites in View
  else if (msgType.endsWith("GSV") && fields.size() >= 4) {
    int totalMessages = fields[1].toInt();
    int messageNumber = fields[2].toInt();
    int satellitesInView = fields[3].toInt();

    if (!isnan(satellitesInView) && satellitesInView >= 0) {
      setPathValue("navigation.gnss.satellitesInView", (double)satellitesInView, "nmea0183.GPS", "", "Satellites in view");
    }
  }
}

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

// ====== WEBSOCKET EVENT HANDLER ======
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

            // Extract anchor position
            if (obj.containsKey("anchor")) {
              JsonObject anchor = obj["anchor"];
              if (anchor.containsKey("lat") && anchor.containsKey("lon")) {
                geofence.anchorLat = anchor["lat"];
                geofence.anchorLon = anchor["lon"];
                geofence.anchorTimestamp = millis();
                Serial.printf("WS: Anchor set: %.6f, %.6f\n", geofence.anchorLat, geofence.anchorLon);
              }
              if (anchor.containsKey("radius")) {
                geofence.radius = anchor["radius"];
                Serial.printf("WS: Geofence radius: %.0f m\n", geofence.radius);
              }
              if (anchor.containsKey("enabled")) {
                geofence.enabled = anchor["enabled"];
                Serial.printf("WS: Geofence enabled: %s\n", geofence.enabled ? "true" : "false");
              }
            }

            // Extract depth alarm config
            if (obj.containsKey("depth")) {
              JsonObject depth = obj["depth"];
              if (depth.containsKey("min_depth")) {
                depthAlarm.threshold = depth["min_depth"];
                Serial.printf("WS: Depth threshold: %.1f m\n", depthAlarm.threshold);
              }
              if (depth.containsKey("alarm")) {
                depthAlarm.enabled = depth["alarm"];
                Serial.printf("WS: Depth alarm enabled: %s\n", depthAlarm.enabled ? "true" : "false");
              }
            }

            // Extract wind alarm config
            if (obj.containsKey("wind")) {
              JsonObject wind = obj["wind"];
              if (wind.containsKey("max_speed")) {
                windAlarm.threshold = wind["max_speed"];
                Serial.printf("WS: Wind threshold: %.1f kn\n", windAlarm.threshold);
              }
              if (wind.containsKey("alarm")) {
                windAlarm.enabled = wind["alarm"];
                Serial.printf("WS: Wind alarm enabled: %s\n", windAlarm.enabled ? "true" : "false");
              }
            }
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

// ====== HTTP HANDLERS ======

// GET /signalk - Server info
void handleSignalKRoot(AsyncWebServerRequest* req) {
  Serial.println("\n=== /signalk DISCOVERY REQUEST ===");
  Serial.printf("Client IP: %s\n", req->client()->remoteIP().toString().c_str());

  DynamicJsonDocument doc(1024);

  JsonObject endpoints = doc.createNestedObject("endpoints");
  JsonObject v1 = endpoints.createNestedObject("v1");

  v1["version"] = "1.7.0";
  v1["signalk-http"] = "http://" + WiFi.localIP().toString() + ":3000/signalk/v1/api/";
  v1["signalk-ws"] = "ws://" + WiFi.localIP().toString() + ":3000/signalk/v1/stream";

  JsonObject server = doc.createNestedObject("server");
  server["id"] = serverName;
  server["version"] = "1.0.0";

  // Security configuration removed - no authentication required
  // This allows SensESP to connect without Authorization headers,
  // avoiding AsyncWebSocket library bug with bearer tokens

  String output;
  serializeJson(doc, output);

  Serial.println("Response:");
  Serial.println(output);
  Serial.println("==================================\n");

  req->send(200, "application/json", output);
}

// GET /signalk/v1/api/ - API root
void handleAPIRoot(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(512);
  doc["name"] = serverName;
  doc["version"] = "1.7.0";
  doc["self"] = "vessels." + vesselUUID;

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

// POST /signalk/v1/access/requests - Handle access requests (auto-approve)
void handleAccessRequest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  // Only process when we have the complete body
  if (index + len != total) {
    return;
  }

  Serial.println("\n=== ACCESS REQUEST (POST) ===");
  Serial.printf("Body: %.*s\n", len, data);

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, data, len);

  String clientId = doc["clientId"] | "unknown";
  String description = doc["description"] | "SensESP client";
  String permissions = doc["permissions"] | "readwrite";

  Serial.printf("Client ID: %s\n", clientId.c_str());
  Serial.printf("Description: %s\n", description.c_str());
  Serial.printf("Permissions: %s\n", permissions.c_str());

  // Generate a requestId (use clientId as requestId for simplicity)
  String requestId = clientId;

  // Check if this client already has an approved token
  for (auto& pair : approvedTokens) {
    if (pair.second.clientId == clientId) {
      Serial.printf("Client already has approved token: %s\n", pair.second.token.c_str());

      // Return COMPLETED with existing token
      DynamicJsonDocument response(512);
      response["state"] = "COMPLETED";
      response["statusCode"] = 200;
      response["requestId"] = requestId;

      JsonObject accessRequest = response.createNestedObject("accessRequest");
      accessRequest["permission"] = "APPROVED";
      accessRequest["token"] = pair.second.token;

      String output;
      serializeJson(response, output);
      req->send(202, "application/json", output);
      Serial.println("=================================\n");
      return;
    }
  }

  // Create new pending request
  AccessRequestData reqData;
  reqData.requestId = requestId;
  reqData.clientId = clientId;
  reqData.description = description;
  reqData.permissions = permissions;
  reqData.token = ""; // Will be generated upon approval
  reqData.state = "PENDING";
  reqData.permission = "";
  reqData.timestamp = millis();
  accessRequests[requestId] = reqData;

  // SignalK spec: Initial POST returns 202 with PENDING state and href for polling
  DynamicJsonDocument response(256);
  response["state"] = "PENDING";
  response["requestId"] = requestId;
  response["href"] = "/signalk/v1/access/requests/" + requestId;

  String output;
  serializeJson(response, output);

  Serial.printf("Response (202 PENDING): %s\n", output.c_str());
  Serial.printf("RequestId: %s - Awaiting manual approval\n", requestId.c_str());
  Serial.println("Access admin UI at http://<ip>:3000/admin to approve/deny");
  Serial.println("=================================\n");

  req->send(202, "application/json", output);
}

// GET /signalk/v1/access/requests/{requestId} - Poll for access request status
void handleGetAccessRequestById(AsyncWebServerRequest* req) {
  String uri = req->url();
  Serial.printf("\n=== GET ACCESS REQUEST: %s ===\n", uri.c_str());

  // Extract requestId from URL path
  // URL format: /signalk/v1/access/requests/{requestId}
  int lastSlash = uri.lastIndexOf('/');
  String requestId = uri.substring(lastSlash + 1);

  Serial.printf("RequestId: %s\n", requestId.c_str());

  // Check if requestId exists
  if (accessRequests.find(requestId) == accessRequests.end()) {
    Serial.println("RequestId not found");
    req->send(404, "application/json", "{\"error\":\"Request not found\"}");
    return;
  }

  AccessRequestData& reqData = accessRequests[requestId];

  // Build response based on state
  DynamicJsonDocument response(512);
  response["state"] = reqData.state;
  response["requestId"] = requestId;

  if (reqData.state == "COMPLETED") {
    response["statusCode"] = 200;
    JsonObject accessRequest = response.createNestedObject("accessRequest");
    accessRequest["permission"] = reqData.permission; // "APPROVED" or "DENIED"

    if (reqData.permission == "APPROVED") {
      accessRequest["token"] = reqData.token;
    }

    Serial.printf("Response: %s with token: %s\n", reqData.permission.c_str(), reqData.token.c_str());
  } else {
    // Still PENDING
    response["href"] = "/signalk/v1/access/requests/" + requestId;
    Serial.println("Response: Still PENDING");
  }

  String output;
  serializeJson(response, output);

  Serial.printf("Response JSON: %s\n", output.c_str());
  Serial.println("==============================\n");

  req->send(200, "application/json", output);
}

// GET /signalk/v1/access/requests - List access requests (always return empty)
void handleGetAccessRequests(AsyncWebServerRequest* req) {
  String url = req->url();

  // Check if this is actually a request for a specific requestId
  // URL: /signalk/v1/access/requests/test-client-12345
  if (url.length() > 28 && url.startsWith("/signalk/v1/access/requests/")) {
    // This is a request for a specific ID, route to the correct handler
    Serial.println("=== Detected requestId in URL, routing to handleGetAccessRequestById ===");
    Serial.printf("URL: %s\n", url.c_str());
    handleGetAccessRequestById(req);
    return;
  }

  // Return empty array - no pending requests since we auto-approve
  Serial.println("=== GET /signalk/v1/access/requests - Returning empty list ===");
  req->send(200, "application/json", "[]");
}

// GET /signalk/v1/auth/validate - Validate token (always valid)
void handleAuthValidate(AsyncWebServerRequest* req) {
  // Check if Authorization header is present
  if (req->hasHeader("Authorization")) {
    String auth = req->header("Authorization");
    Serial.printf("Token validation request: %s\n", auth.c_str());
  }

  // Always return valid
  DynamicJsonDocument doc(256);
  doc["valid"] = true;
  doc["state"] = "COMPLETED";
  doc["statusCode"] = 200;

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

// GET /signalk/v1/api/vessels/self
void handleVesselsSelf(AsyncWebServerRequest* req) {
  Serial.println("\n=== GET /signalk/v1/api/vessels/self ===");
  Serial.printf("dataStore has %d items\n", dataStore.size());

  DynamicJsonDocument doc(4096);

  doc["uuid"] = vesselUUID;
  doc["name"] = serverName;

  // Navigation data
  if (dataStore.size() > 0) {
    JsonObject nav = doc.createNestedObject("navigation");

    for (auto& kv : dataStore) {
      String path = kv.first;
      if (!path.startsWith("navigation.")) continue;
      Serial.printf("Processing navigation path: %s\n", path.c_str());

      String subPath = path.substring(11); // Remove "navigation."

      // Handle nested paths by creating nested objects
      JsonObject current = nav;

      // Split the path and navigate through all parent parts
      int dotIndex = 0;
      int prevDotIndex = 0;
      while ((dotIndex = subPath.indexOf('.', prevDotIndex)) >= 0) {
        String part = subPath.substring(prevDotIndex, dotIndex);
        if (!current.containsKey(part)) {
          current = current.createNestedObject(part);
        } else {
          JsonVariant v = current[part];
          if (v.is<JsonObject>()) {
            current = v.as<JsonObject>();
          } else {
            // Key exists but is not an object, create nested object
            current = current.createNestedObject(part);
          }
        }
        prevDotIndex = dotIndex + 1;
      }

      // The remaining part is the final key
      String finalKey = subPath.substring(prevDotIndex);
      subPath = finalKey;

      JsonObject value = current.createNestedObject(subPath);
      value["timestamp"] = kv.second.timestamp;

      if (kv.second.isJson) {
        // Parse JSON string and set as object
        DynamicJsonDocument valueDoc(512);
        DeserializationError err = deserializeJson(valueDoc, kv.second.jsonValue);
        if (!err) {
          value["value"] = valueDoc.as<JsonVariant>();
        } else {
          value["value"] = kv.second.jsonValue;
        }
      } else if (kv.second.isNumeric) {
        value["value"] = kv.second.numValue;
      } else {
        value["value"] = kv.second.strValue;
      }

      JsonObject meta = value.createNestedObject("meta");
      if (kv.second.units.length() > 0) meta["units"] = kv.second.units;
      if (kv.second.description.length() > 0) meta["description"] = kv.second.description;

      JsonObject src = value.createNestedObject("$source");
      src["label"] = kv.second.source;
    }
  }

  // Add environment data (depth, wind, etc.)
  JsonObject env = doc.createNestedObject("environment");
  for (auto& kv : dataStore) {
    String path = kv.first;
    if (!path.startsWith("environment.")) continue;

    String subPath = path.substring(12); // Remove "environment."

    // Handle nested paths
    JsonObject current = env;

    // Split the path and navigate through all parent parts
    int dotIndex = 0;
    int prevDotIndex = 0;
    while ((dotIndex = subPath.indexOf('.', prevDotIndex)) >= 0) {
      String part = subPath.substring(prevDotIndex, dotIndex);
      if (!current.containsKey(part)) {
        current = current.createNestedObject(part);
      } else {
        JsonVariant v = current[part];
        if (v.is<JsonObject>()) {
          current = v.as<JsonObject>();
        } else {
          current = current.createNestedObject(part);
        }
      }
      prevDotIndex = dotIndex + 1;
    }

    // The remaining part is the final key
    String finalKey = subPath.substring(prevDotIndex);
    subPath = finalKey;

    JsonObject value = current.createNestedObject(subPath);
    value["timestamp"] = kv.second.timestamp;

    if (kv.second.isJson) {
      DynamicJsonDocument valueDoc(512);
      DeserializationError err = deserializeJson(valueDoc, kv.second.jsonValue);
      if (!err) {
        value["value"] = valueDoc.as<JsonVariant>();
      } else {
        value["value"] = kv.second.jsonValue;
      }
    } else if (kv.second.isNumeric) {
      value["value"] = kv.second.numValue;
    } else {
      value["value"] = kv.second.strValue;
    }

    JsonObject meta = value.createNestedObject("meta");
    if (kv.second.units.length() > 0) meta["units"] = kv.second.units;
    if (kv.second.description.length() > 0) meta["description"] = kv.second.description;

    JsonObject src = value.createNestedObject("$source");
    src["label"] = kv.second.source;
  }

  // Add notifications
  if (!notifications.empty()) {
    JsonObject notifs = doc.createNestedObject("notifications");
    for (auto& kv : dataStore) {
      String path = kv.first;
      if (!path.startsWith("notifications.")) continue;

      String subPath = path.substring(14); // Remove "notifications."

      if (kv.second.isJson) {
        DynamicJsonDocument valueDoc(512);
        DeserializationError err = deserializeJson(valueDoc, kv.second.jsonValue);
        if (!err) {
          notifs[subPath] = valueDoc.as<JsonVariant>();
        }
      }
    }
  }

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

// GET /signalk/v1/api/vessels/self/* - Get specific path
void handleGetPath(AsyncWebServerRequest* req) {
  String path = req->url().substring(String("/signalk/v1/api/vessels/self/").length());
  path.replace("/", ".");
  
  if (dataStore.count(path) == 0) {
    req->send(404, "application/json", "{\"error\":\"Path not found\"}");
    return;
  }
  
  PathValue& pv = dataStore[path];
  DynamicJsonDocument doc(512);

  if (pv.isJson) {
    // Parse JSON string and set as object
    DynamicJsonDocument valueDoc(256);
    DeserializationError err = deserializeJson(valueDoc, pv.jsonValue);
    if (!err) {
      doc["value"] = valueDoc.as<JsonVariant>();
    } else {
      doc["value"] = pv.jsonValue;
    }
  } else if (pv.isNumeric) {
    doc["value"] = pv.numValue;
  } else {
    doc["value"] = pv.strValue;
  }

  doc["timestamp"] = pv.timestamp;
  doc["$source"] = pv.source;
  
  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

// PUT /signalk/v1/api/vessels/self/* - Update path (no auth required)
void handlePutPath(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  // Only process when we have the complete body
  if (index + len != total) {
    return;  // Wait for more data
  }

  String path = req->url().substring(String("/signalk/v1/api/vessels/self/").length());
  path.replace("/", ".");

  Serial.println("\n=== PUT PATH REQUEST ===");
  Serial.printf("Full URL: %s\n", req->url().c_str());
  Serial.printf("Path: %s\n", path.c_str());
  Serial.printf("Data length: %d bytes\n", len);
  Serial.printf("Raw body: %.*s\n", len, data);

  // Optional: Check for authentication token
  // (Currently permissive - allows requests without tokens)
  String token = extractBearerToken(req);
  if (token.length() > 0) {
    if (!isTokenValid(token)) {
      Serial.printf("Invalid token provided: %s\n", token.c_str());
      req->send(401, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":401,\"message\":\"Unauthorized - Invalid token\"}");
      return;
    }
    Serial.printf("Valid token: %s\n", token.substring(0, 15).c_str());
  } else {
    Serial.println("No token provided (open access mode)");
  }

  DynamicJsonDocument doc(2048);  // Increased size for complex anchor values
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    req->send(400, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"Invalid JSON\"}");
    return;
  }

  // Debug: Print parsed JSON
  String debugJson;
  serializeJsonPretty(doc, debugJson);
  Serial.printf("Parsed JSON:\n%s\n", debugJson.c_str());

  // Support both {"value": X} and direct value
  JsonVariant value;
  if (doc.containsKey("value")) {
    value = doc["value"];
  } else {
    value = doc.as<JsonVariant>();
  }

  // Get optional source and description
  String source = doc["source"] | "app";
  String description = doc["description"] | "Set by client";

  // Handle different value types
  if (value.isNull()) {
    req->send(400, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"Value cannot be null\"}");
    return;
  } else if (value.is<double>() || value.is<int>() || value.is<float>()) {
    double numValue = value.as<double>();
    setPathValue(path, numValue, source, "", description);
    Serial.printf("Set numeric value: %.6f\n", numValue);
  } else if (value.is<bool>()) {
    // Store boolean as 0 or 1
    double boolValue = value.as<bool>() ? 1.0 : 0.0;
    setPathValue(path, boolValue, source, "", description);
    Serial.printf("Set boolean value: %s\n", value.as<bool>() ? "true" : "false");
  } else if (value.is<const char*>() || value.is<String>()) {
    String strValue = value.as<String>();
    setPathValue(path, strValue, source, "", description);
    Serial.printf("Set string value: %s\n", strValue.c_str());
  } else if (value.is<JsonObject>() || value.is<JsonArray>()) {
    // For complex objects, serialize to string and store as JSON
    String jsonStr;
    serializeJson(value, jsonStr);
    setPathValueJson(path, jsonStr, source, "", description);
    Serial.printf("Set object/array value: %s\n", jsonStr.c_str());

    // Special handling for navigation.anchor.akat (6pack app config)
    if (path == "navigation.anchor.akat" && value.is<JsonObject>()) {
      JsonObject obj = value.as<JsonObject>();

      // Extract anchor position
      if (obj.containsKey("anchor")) {
        JsonObject anchor = obj["anchor"];
        if (anchor.containsKey("lat") && anchor.containsKey("lon")) {
          geofence.anchorLat = anchor["lat"];
          geofence.anchorLon = anchor["lon"];
          geofence.anchorTimestamp = millis();
          Serial.printf("Anchor set: %.6f, %.6f\n", geofence.anchorLat, geofence.anchorLon);
        }
        if (anchor.containsKey("radius")) {
          geofence.radius = anchor["radius"];
          Serial.printf("Geofence radius: %.0f m\n", geofence.radius);
        }
        if (anchor.containsKey("enabled")) {
          geofence.enabled = anchor["enabled"];
          Serial.printf("Geofence enabled: %s\n", geofence.enabled ? "true" : "false");
        }
      }

      // Extract depth alarm config
      if (obj.containsKey("depth")) {
        JsonObject depth = obj["depth"];
        if (depth.containsKey("min_depth")) {
          depthAlarm.threshold = depth["min_depth"];
          Serial.printf("Depth threshold: %.1f m\n", depthAlarm.threshold);
        }
        if (depth.containsKey("alarm")) {
          depthAlarm.enabled = depth["alarm"];
          Serial.printf("Depth alarm enabled: %s\n", depthAlarm.enabled ? "true" : "false");
        }
      }

      // Extract wind alarm config
      if (obj.containsKey("wind")) {
        JsonObject wind = obj["wind"];
        if (wind.containsKey("max_speed")) {
          windAlarm.threshold = wind["max_speed"];
          Serial.printf("Wind threshold: %.1f kn\n", windAlarm.threshold);
        }
        if (wind.containsKey("alarm")) {
          windAlarm.enabled = wind["alarm"];
          Serial.printf("Wind alarm enabled: %s\n", windAlarm.enabled ? "true" : "false");
        }
      }
    }
  } else {
    Serial.println("Unsupported value type");
    req->send(400, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"Unsupported value type\"}");
    return;
  }

  Serial.println("Path updated successfully");
  Serial.println("=======================\n");

  DynamicJsonDocument response(256);
  response["state"] = "COMPLETED";
  response["statusCode"] = 200;

  String output;
  serializeJson(response, output);
  req->send(200, "application/json", output);
}

// ====== ADMIN UI FOR ACCESS REQUESTS ======
// Authentication removed - no admin UI needed

// ====== MAIN WEB UI ======
const char* HTML_UI = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>SignalK Server</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 1200px; margin: 0 auto; }
    h1 { font-size: 24px; margin-bottom: 20px; color: #333; }
    .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .status { display: inline-block; padding: 4px 12px; border-radius: 12px; font-size: 14px; font-weight: 500; }
    .status.connected { background: #4caf50; color: white; }
    .status.disconnected { background: #f44336; color: white; }
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { text-align: left; padding: 12px 8px; border-bottom: 1px solid #eee; font-size: 14px; }
    th { font-weight: 600; color: #666; background: #f9f9f9; }
    code { background: #f0f0f0; padding: 2px 6px; border-radius: 3px; font-family: monospace; font-size: 13px; }
    .value { font-weight: 500; color: #2196f3; }
    .timestamp { color: #999; font-size: 12px; }
    a { color: #2196f3; text-decoration: none; font-weight: 500; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🚢 SignalK Server - ESP32</h1>
    <div style="margin-bottom: 20px;">
      <a href="/config">⚙️ TCP Configuration</a>
    </div>
    
    <div class="card">
      <strong>WebSocket:</strong> <span id="ws-status" class="status disconnected">Disconnected</span>
      <br><br>
      <strong>Server:</strong> <code id="server-url"></code><br>
      <strong>Vessel UUID:</strong> <code id="vessel-uuid"></code>
    </div>
    
    <div class="card">
      <h2 style="font-size: 18px; margin-bottom: 15px;">Navigation Data</h2>
      <table id="data-table">
        <thead>
          <tr>
            <th>Path</th>
            <th>Value</th>
            <th>Units</th>
            <th>Timestamp</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
  </div>
  
  <script>
    const wsUrl = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/signalk/v1/stream';
    const ws = new WebSocket(wsUrl);
    const statusEl = document.getElementById('ws-status');
    const tbody = document.querySelector('#data-table tbody');
    const paths = new Map();
    
    document.getElementById('server-url').textContent = location.origin + '/signalk/v1/api/';
    
    ws.onopen = () => {
      statusEl.textContent = 'Connected';
      statusEl.className = 'status connected';
      
      ws.send(JSON.stringify({
        context: '*',
        subscribe: [{ path: '*', period: 1000, format: 'delta', policy: 'instant' }]
      }));
    };
    
    ws.onclose = () => {
      statusEl.textContent = 'Disconnected';
      statusEl.className = 'status disconnected';
    };
    
    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        
        if (msg.self) {
          document.getElementById('vessel-uuid').textContent = msg.self;
        }
        
        if (msg.updates) {
          msg.updates.forEach(update => {
            if (update.values) {
              update.values.forEach(item => {
                const path = item.path;
                let value = item.value;
                
                if (typeof value === 'number') {
                  value = value.toFixed(6);
                } else if (typeof value === 'object') {
                  value = JSON.stringify(value);
                }
                
                paths.set(path, {
                  value: value,
                  timestamp: update.timestamp || '',
                  units: item.units || ''
                });
              });
            }
          });
          
          renderTable();
        }
      } catch (e) {
        console.error('Error parsing message:', e);
      }
    };
    
    function renderTable() {
      tbody.innerHTML = '';
      
      const sortedPaths = Array.from(paths.entries()).sort((a, b) => a[0].localeCompare(b[0]));
      
      sortedPaths.forEach(([path, data]) => {
        const tr = document.createElement('tr');
        
        const pathTd = document.createElement('td');
        pathTd.innerHTML = `<code>${path}</code>`;
        
        const valueTd = document.createElement('td');
        valueTd.innerHTML = `<span class="value">${data.value}</span>`;
        
        const unitsTd = document.createElement('td');
        unitsTd.textContent = data.units;
        
        const tsTd = document.createElement('td');
        tsTd.innerHTML = `<span class="timestamp">${data.timestamp}</span>`;
        
        tr.appendChild(pathTd);
        tr.appendChild(valueTd);
        tr.appendChild(unitsTd);
        tr.appendChild(tsTd);
        
        tbody.appendChild(tr);
      });
    }
  </script>
</body>
</html>
)html";

void handleRoot(AsyncWebServerRequest* req) {
  req->send(200, "text/html", HTML_UI);
}

// ====== TCP CONFIGURATION PAGE ======
const char* HTML_CONFIG = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>TCP Configuration - SignalK</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; }
    h1 { font-size: 24px; margin-bottom: 20px; color: #333; }
    .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    label { display: block; margin-bottom: 8px; font-weight: 500; color: #333; }
    input[type="text"], input[type="number"] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; margin-bottom: 16px; }
    .checkbox-wrapper { margin-bottom: 20px; }
    input[type="checkbox"] { width: 20px; height: 20px; margin-right: 8px; cursor: pointer; }
    button { background: #2196f3; color: white; padding: 12px 24px; border: none; border-radius: 4px; font-size: 16px; font-weight: 500; cursor: pointer; width: 100%; margin-bottom: 10px; }
    button:hover { background: #1976d2; }
    button.secondary { background: #757575; }
    button.secondary:hover { background: #616161; }
    .info { background: #e3f2fd; padding: 12px; border-radius: 4px; margin-bottom: 20px; font-size: 14px; color: #1976d2; }
    .status { display: inline-block; padding: 4px 12px; border-radius: 12px; font-size: 14px; font-weight: 500; margin-top: 12px; }
    .status.connected { background: #4caf50; color: white; }
    .status.disconnected { background: #f44336; color: white; }
    a { color: #2196f3; text-decoration: none; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚙️ TCP Configuration</h1>
    <div style="margin-bottom: 20px;">
      <a href="/">← Back to Main</a>
    </div>

    <div class="info">
      Configure connection to an external SignalK server via TCP. The server will receive and display data from the external source. Typical SignalK TCP port is 10110.<br><br>
      <strong>Note:</strong> The ESP32 also runs an NMEA TCP server on port 10110 for receiving mock NMEA data. You can connect to it from another device to send NMEA sentences for testing.
    </div>

    <div class="card">
      <form id="tcp-form">
        <label for="host">Server Hostname or IP:</label>
        <input type="text" id="host" name="host" placeholder="signalk.example.com or 192.168.1.100" required>

        <label for="port">Port:</label>
        <input type="number" id="port" name="port" value="10110" min="1" max="65535" required>

        <div class="checkbox-wrapper">
          <label style="display: inline-flex; align-items: center; cursor: pointer;">
            <input type="checkbox" id="enabled" name="enabled">
            <span>Enable TCP Connection</span>
          </label>
        </div>

        <button type="submit">Save Configuration</button>
        <button type="button" class="secondary" onclick="location.href='/'">Cancel</button>
      </form>

      <div id="status-display"></div>
    </div>

    <div class="card">
      <h2 style="font-size: 18px; margin-bottom: 12px;">Current Configuration</h2>
      <p><strong>Host:</strong> <span id="current-host">-</span></p>
      <p><strong>Port:</strong> <span id="current-port">-</span></p>
      <p><strong>Enabled:</strong> <span id="current-enabled">-</span></p>
    </div>

    <div class="card">
      <h2 style="font-size: 18px; margin-bottom: 12px;">NMEA TCP Server</h2>
      <p style="font-size: 14px; color: #666; line-height: 1.6;">
        The ESP32 runs an NMEA TCP server on port 10110 that accepts connections from NMEA data sources. You can use tools like netcat, telnet, or custom scripts to send NMEA sentences to this port for testing.
      </p>
      <p style="font-size: 14px; color: #666; line-height: 1.6; margin-top: 8px;">
        <strong>Example:</strong> <code>echo '$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A' | nc 192.168.4.1 10110</code>
      </p>
    </div>

    <div class="card">
      <h2 style="font-size: 18px; margin-bottom: 12px;">Troubleshooting</h2>
      <p style="font-size: 14px; color: #666; line-height: 1.6;">
        If the connection fails:
      </p>
      <ul style="font-size: 14px; color: #666; line-height: 1.8; margin-left: 20px; margin-top: 8px;">
        <li>Ensure your ESP32 is connected to the internet via WiFiManager</li>
        <li>Verify the hostname/IP is correct and accessible from your network</li>
        <li>Check that port 10110 (or your configured port) is open on the remote server</li>
        <li>Monitor the serial console for detailed connection logs</li>
        <li>Try using an IP address instead of hostname to rule out DNS issues</li>
      </ul>
    </div>
  </div>

  <script>
    // Load current configuration
    fetch('/api/tcp/config')
      .then(r => r.json())
      .then(data => {
        document.getElementById('host').value = data.host || '';
        document.getElementById('port').value = data.port || 10110;
        document.getElementById('enabled').checked = data.enabled || false;

        document.getElementById('current-host').textContent = data.host || '(not set)';
        document.getElementById('current-port').textContent = data.port || 10110;
        document.getElementById('current-enabled').textContent = data.enabled ? 'Yes' : 'No';
      })
      .catch(err => console.error('Failed to load config:', err));

    // Handle form submission
    document.getElementById('tcp-form').addEventListener('submit', async (e) => {
      e.preventDefault();

      const config = {
        host: document.getElementById('host').value,
        port: parseInt(document.getElementById('port').value),
        enabled: document.getElementById('enabled').checked
      };

      try {
        const response = await fetch('/api/tcp/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(config)
        });

        if (response.ok) {
          document.getElementById('status-display').innerHTML =
            '<div class="status connected">Configuration saved successfully!</div>';

          // Update current config display
          document.getElementById('current-host').textContent = config.host || '(not set)';
          document.getElementById('current-port').textContent = config.port;
          document.getElementById('current-enabled').textContent = config.enabled ? 'Yes' : 'No';

          setTimeout(() => {
            document.getElementById('status-display').innerHTML = '';
          }, 3000);
        } else {
          document.getElementById('status-display').innerHTML =
            '<div class="status disconnected">Failed to save configuration</div>';
        }
      } catch (err) {
        document.getElementById('status-display').innerHTML =
          '<div class="status disconnected">Error: ' + err.message + '</div>';
      }
    });
  </script>
</body>
</html>
)html";

void handleConfig(AsyncWebServerRequest* req) {
  req->send(200, "text/html", HTML_CONFIG);
}

// ====== ADMIN TOKEN MANAGEMENT PAGE ======
const char* HTML_ADMIN = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Token Management - SignalK</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 900px; margin: 0 auto; }
    .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h1 { color: #333; margin-bottom: 10px; font-size: 24px; }
    h2 { color: #666; margin-bottom: 15px; font-size: 18px; }
    table { width: 100%; border-collapse: collapse; }
    th, td { text-align: left; padding: 12px; border-bottom: 1px solid #eee; }
    th { background: #f8f8f8; font-weight: 600; }
    .btn { padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
    .btn-approve { background: #4CAF50; color: white; }
    .btn-deny { background: #f44336; color: white; }
    .btn-revoke { background: #ff9800; color: white; }
    .btn:hover { opacity: 0.9; }
    .status { display: inline-block; padding: 4px 8px; border-radius: 4px; font-size: 12px; font-weight: 600; }
    .status-pending { background: #fff3cd; color: #856404; }
    .status-approved { background: #d4edda; color: #155724; }
    .empty { text-align: center; color: #999; padding: 20px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h1>Token Management</h1>
      <p style="color: #666; margin-top: 10px;">Manage access tokens for SignalK clients</p>
    </div>

    <div class="card">
      <h2>Pending Requests</h2>
      <div id="pending-requests"></div>
    </div>

    <div class="card">
      <h2>Approved Tokens</h2>
      <div id="approved-tokens"></div>
    </div>
  </div>

  <script>
    function loadData() {
      fetch('/api/admin/tokens')
        .then(r => r.json())
        .then(data => {
          renderPending(data.pending);
          renderApproved(data.approved);
        });
    }

    function renderPending(requests) {
      const el = document.getElementById('pending-requests');
      if (!requests || requests.length === 0) {
        el.innerHTML = '<div class="empty">No pending requests</div>';
        return;
      }

      el.innerHTML = '<table><tr><th>Client ID</th><th>Description</th><th>Permissions</th><th>Actions</th></tr>' +
        requests.map(r => `
          <tr>
            <td><strong>${r.clientId}</strong></td>
            <td>${r.description}</td>
            <td>${r.permissions}</td>
            <td>
              <button class="btn btn-approve" onclick="approve('${r.requestId}')">Approve</button>
              <button class="btn btn-deny" onclick="deny('${r.requestId}')">Deny</button>
            </td>
          </tr>
        `).join('') + '</table>';
    }

    function renderApproved(tokens) {
      const el = document.getElementById('approved-tokens');
      if (!tokens || tokens.length === 0) {
        el.innerHTML = '<div class="empty">No approved tokens</div>';
        return;
      }

      el.innerHTML = '<table><tr><th>Client ID</th><th>Description</th><th>Token</th><th>Actions</th></tr>' +
        tokens.map(t => `
          <tr>
            <td><strong>${t.clientId}</strong></td>
            <td>${t.description}</td>
            <td><code style="font-size:11px">${t.token.substring(0,20)}...</code></td>
            <td>
              <button class="btn btn-revoke" onclick="revoke('${t.token}')">Revoke</button>
            </td>
          </tr>
        `).join('') + '</table>';
    }

    function approve(requestId) {
      if (!confirm('Approve this access request?')) return;
      fetch(`/api/admin/approve/${requestId}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Approved!'); loadData(); });
    }

    function deny(requestId) {
      if (!confirm('Deny this access request?')) return;
      fetch(`/api/admin/deny/${requestId}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Denied!'); loadData(); });
    }

    function revoke(token) {
      if (!confirm('Revoke this token? The client will lose access.')) return;
      fetch(`/api/admin/revoke/${token}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Token revoked!'); loadData(); });
    }

    loadData();
    setInterval(loadData, 5000); // Refresh every 5 seconds
  </script>
</body>
</html>
)html";

void handleAdmin(AsyncWebServerRequest* req) {
  req->send(200, "text/html", HTML_ADMIN);
}

// API: Get all tokens and pending requests
void handleGetAdminTokens(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(2048);

  // Pending requests
  JsonArray pending = doc.createNestedArray("pending");
  for (auto& pair : accessRequests) {
    if (pair.second.state == "PENDING") {
      JsonObject obj = pending.createNestedObject();
      obj["requestId"] = pair.second.requestId;
      obj["clientId"] = pair.second.clientId;
      obj["description"] = pair.second.description;
      obj["permissions"] = pair.second.permissions;
    }
  }

  // Approved tokens
  JsonArray approved = doc.createNestedArray("approved");
  for (auto& pair : approvedTokens) {
    JsonObject obj = approved.createNestedObject();
    obj["token"] = pair.second.token;
    obj["clientId"] = pair.second.clientId;
    obj["description"] = pair.second.description;
    obj["permissions"] = pair.second.permissions;
  }

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

// Admin API router - handles all /api/admin/* POST routes
void handleAdminApiPost(AsyncWebServerRequest* req) {
  String url = req->url();
  Serial.printf("\n=== ADMIN API POST: %s ===\n", url.c_str());

  // Route to appropriate handler
  if (url.startsWith("/api/admin/approve/")) {
    String requestId = url.substring(19); // Skip "/api/admin/approve/"
    Serial.printf("Routing to APPROVE: %s\n", requestId.c_str());

    if (accessRequests.find(requestId) == accessRequests.end()) {
      req->send(404, "application/json", "{\"error\":\"Request not found\"}");
      return;
    }

    AccessRequestData& reqData = accessRequests[requestId];

    // Generate token
    String token = "APPROVED-" + String(esp_random(), HEX);

    // Update request
    reqData.token = token;
    reqData.state = "COMPLETED";
    reqData.permission = "APPROVED";

    // Add to approved tokens
    ApprovedToken approvedToken;
    approvedToken.token = token;
    approvedToken.clientId = reqData.clientId;
    approvedToken.description = reqData.description;
    approvedToken.permissions = reqData.permissions;
    approvedToken.approvedAt = millis();
    approvedTokens[token] = approvedToken;

    // Save to flash
    saveApprovedTokens();

    Serial.printf("Token approved: %s\n", token.c_str());
    Serial.printf("Client: %s\n", reqData.clientId.c_str());
    Serial.println("================================\n");

    DynamicJsonDocument response(256);
    response["success"] = true;
    response["token"] = token;

    String output;
    serializeJson(response, output);
    req->send(200, "application/json", output);
    return;
  }

  if (url.startsWith("/api/admin/deny/")) {
    String requestId = url.substring(16); // Skip "/api/admin/deny/"
    Serial.printf("Routing to DENY: %s\n", requestId.c_str());

    if (accessRequests.find(requestId) == accessRequests.end()) {
      req->send(404, "application/json", "{\"error\":\"Request not found\"}");
      return;
    }

    AccessRequestData& reqData = accessRequests[requestId];
    reqData.state = "COMPLETED";
    reqData.permission = "DENIED";

    Serial.printf("Request denied for client: %s\n", reqData.clientId.c_str());
    Serial.println("================================\n");

    req->send(200, "application/json", "{\"success\":true}");
    return;
  }

  if (url.startsWith("/api/admin/revoke/")) {
    String token = url.substring(18); // Skip "/api/admin/revoke/"
    Serial.printf("Routing to REVOKE: %s\n", token.c_str());

    if (approvedTokens.find(token) == approvedTokens.end()) {
      req->send(404, "application/json", "{\"error\":\"Token not found\"}");
      return;
    }

    String clientId = approvedTokens[token].clientId;
    approvedTokens.erase(token);

    // Save to flash
    saveApprovedTokens();

    Serial.printf("Token revoked for client: %s\n", clientId.c_str());
    Serial.println("================================\n");

    req->send(200, "application/json", "{\"success\":true}");
    return;
  }

  // Unknown admin API route
  req->send(404, "application/json", "{\"error\":\"Unknown admin API route\"}");
}

// API: Approve token request
void handleApproveToken(AsyncWebServerRequest* req) {
  String url = req->url();
  int lastSlash = url.lastIndexOf('/');
  String requestId = url.substring(lastSlash + 1);

  Serial.printf("\n=== APPROVE REQUEST: %s ===\n", requestId.c_str());

  if (accessRequests.find(requestId) == accessRequests.end()) {
    req->send(404, "application/json", "{\"error\":\"Request not found\"}");
    return;
  }

  AccessRequestData& reqData = accessRequests[requestId];

  // Generate token
  String token = "APPROVED-" + String(esp_random(), HEX);

  // Update request
  reqData.token = token;
  reqData.state = "COMPLETED";
  reqData.permission = "APPROVED";

  // Add to approved tokens
  ApprovedToken approvedToken;
  approvedToken.token = token;
  approvedToken.clientId = reqData.clientId;
  approvedToken.description = reqData.description;
  approvedToken.permissions = reqData.permissions;
  approvedToken.approvedAt = millis();
  approvedTokens[token] = approvedToken;

  // Save to flash
  saveApprovedTokens();

  Serial.printf("Token approved: %s\n", token.c_str());
  Serial.printf("Client: %s\n", reqData.clientId.c_str());
  Serial.println("================================\n");

  DynamicJsonDocument response(256);
  response["success"] = true;
  response["token"] = token;

  String output;
  serializeJson(response, output);
  req->send(200, "application/json", output);
}

// API: Deny token request
void handleDenyToken(AsyncWebServerRequest* req) {
  String url = req->url();
  int lastSlash = url.lastIndexOf('/');
  String requestId = url.substring(lastSlash + 1);

  Serial.printf("\n=== DENY REQUEST: %s ===\n", requestId.c_str());

  if (accessRequests.find(requestId) == accessRequests.end()) {
    req->send(404, "application/json", "{\"error\":\"Request not found\"}");
    return;
  }

  AccessRequestData& reqData = accessRequests[requestId];
  reqData.state = "COMPLETED";
  reqData.permission = "DENIED";

  Serial.printf("Request denied for client: %s\n", reqData.clientId.c_str());
  Serial.println("================================\n");

  req->send(200, "application/json", "{\"success\":true}");
}

// API: Revoke token
void handleRevokeToken(AsyncWebServerRequest* req) {
  String url = req->url();
  int lastSlash = url.lastIndexOf('/');
  String token = url.substring(lastSlash + 1);

  Serial.printf("\n=== REVOKE TOKEN: %s ===\n", token.c_str());

  if (approvedTokens.find(token) == approvedTokens.end()) {
    req->send(404, "application/json", "{\"error\":\"Token not found\"}");
    return;
  }

  String clientId = approvedTokens[token].clientId;
  approvedTokens.erase(token);

  // Save to flash
  saveApprovedTokens();

  Serial.printf("Token revoked for client: %s\n", clientId.c_str());
  Serial.println("================================\n");

  req->send(200, "application/json", "{\"success\":true}");
}

// API: Register Expo push token
void handleRegisterExpoToken(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  // Only process when we have the complete body
  if (index + len != total) {
    return;
  }

  Serial.println("\n=== REGISTER EXPO TOKEN ===");

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  String token = doc["token"] | "";

  if (token.length() == 0) {
    Serial.println("No token provided");
    req->send(400, "application/json", "{\"error\":\"Token required\"}");
    return;
  }

  // Validate token format (should start with "ExponentPushToken[")
  if (!token.startsWith("ExponentPushToken[")) {
    Serial.println("Invalid token format");
    req->send(400, "application/json", "{\"error\":\"Invalid token format\"}");
    return;
  }

  bool added = addExpoToken(token);

  Serial.printf("Token: %s\n", token.c_str());
  Serial.printf("Status: %s\n", added ? "Added" : "Already exists");
  Serial.println("================================\n");

  DynamicJsonDocument response(128);
  response["success"] = true;
  response["added"] = added;
  response["totalTokens"] = expoTokens.size();

  String output;
  serializeJson(response, output);
  req->send(200, "application/json", output);
}

void handleGetTcpConfig(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(256);
  doc["host"] = tcpServerHost;
  doc["port"] = tcpServerPort;
  doc["enabled"] = tcpEnabled;

  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

void handleSetTcpConfig(AsyncWebServerRequest* req, uint8_t *data, size_t len, size_t index, size_t total) {
  // Only process when we have the complete body
  if (index + len != total) {
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  String host = doc["host"] | "";
  int port = doc["port"] | 10110;
  bool enabled = doc["enabled"] | false;

  // Save configuration inline (since saveTcpConfig is defined later)
  prefs.begin("signalk", false);
  prefs.putString("tcp_host", host);
  prefs.putInt("tcp_port", port);
  prefs.putBool("tcp_enabled", enabled);
  prefs.end();

  tcpServerHost = host;
  tcpServerPort = port;
  tcpEnabled = enabled;

  Serial.println("\n=== TCP Configuration Saved ===");
  Serial.println("Host: " + host);
  Serial.println("Port: " + String(port));
  Serial.println("Enabled: " + String(enabled ? "Yes" : "No"));
  Serial.println("===============================\n");

  // Disconnect if currently connected
  if (tcpClient.connected()) {
    tcpClient.stop();
  }

  req->send(200, "application/json", "{\"success\":true}");
}

// ====== TCP CLIENT FUNCTIONS ======
void loadTcpConfig() {
  prefs.begin("signalk", false);
  tcpServerHost = prefs.getString("tcp_host", "");
  tcpServerPort = prefs.getInt("tcp_port", 10110);
  tcpEnabled = prefs.getBool("tcp_enabled", false);
  prefs.end();

  Serial.println("\n=== TCP Configuration ===");
  Serial.println("Host: " + tcpServerHost);
  Serial.println("Port: " + String(tcpServerPort));
  Serial.println("Enabled: " + String(tcpEnabled ? "Yes" : "No"));
  Serial.println("=========================\n");
}

void saveTcpConfig(String host, int port, bool enabled) {
  prefs.begin("signalk", false);
  prefs.putString("tcp_host", host);
  prefs.putInt("tcp_port", port);
  prefs.putBool("tcp_enabled", enabled);
  prefs.end();

  tcpServerHost = host;
  tcpServerPort = port;
  tcpEnabled = enabled;

  Serial.println("\n=== TCP Configuration Saved ===");
  Serial.println("Host: " + host);
  Serial.println("Port: " + String(port));
  Serial.println("Enabled: " + String(enabled ? "Yes" : "No"));
  Serial.println("===============================\n");

  // Disconnect if currently connected
  if (tcpClient.connected()) {
    tcpClient.stop();
  }
}

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

  // Removed aggressive connection quality check that was causing false disconnects
  // The standard tcpClient.connected() check below is sufficient

  // Read available data - process NMEA sentences from TCP
  while (tcpClient.available()) {
    char c = tcpClient.read();

    // NMEA sentences end with newline
    if (c == '\n' || c == '\r') {
      if (tcpBuffer.length() > 0) {
        if (tcpBuffer[0] == '$') {
          // This is an NMEA sentence - parse it
          // Serial.println("TCP NMEA: " + tcpBuffer);
          parseNMEASentence(tcpBuffer);
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

void processNmeaTcpServer() {
  // Check for new client connections
  if (!nmeaTcpClient || !nmeaTcpClient.connected()) {
    // Clean up existing client first
    if (nmeaTcpClient) {
      nmeaTcpClient.stop();
    }

    nmeaTcpClient = nmeaTcpServer.available();
    if (nmeaTcpClient) {
      Serial.println("NMEA TCP Server: New client connected from " + nmeaTcpClient.remoteIP().toString());
      nmeaTcpBuffer = "";
      nmeaLastActivity = millis();
      sentenceCount = 0;
      sentenceWindowStart = millis();
    }
  }

  // Process data from connected client
  if (nmeaTcpClient && nmeaTcpClient.connected()) {
    // Check for rate limiting window reset
    uint32_t now = millis();
    if (now - sentenceWindowStart > 1000) {
      sentenceCount = 0;
      sentenceWindowStart = now;
    }

    // Check for client timeout
    if (nmeaTcpClient.available()) {
      nmeaLastActivity = now;
    } else if (now - nmeaLastActivity > NMEA_CLIENT_TIMEOUT) {
      Serial.println("NMEA TCP Server: Client timeout, disconnecting");
      nmeaTcpClient.stop();
      nmeaTcpBuffer = "";
      return;
    }

    while (nmeaTcpClient.available()) {
      char c = nmeaTcpClient.read();

      // NMEA sentences end with newline
      if (c == '\n' || c == '\r') {
        if (nmeaTcpBuffer.length() > 6 && nmeaTcpBuffer[0] == '$') {
          // Rate limiting check
          if (sentenceCount >= MAX_SENTENCES_PER_SECOND) {
            Serial.println("NMEA TCP Server: Rate limit exceeded, disconnecting client");
            nmeaTcpClient.stop();
            nmeaTcpBuffer = "";
            return;
          }

          // This is an NMEA sentence - parse it
          Serial.println("NMEA TCP Server: " + nmeaTcpBuffer);
          parseNMEASentence(nmeaTcpBuffer);
          sentenceCount++;

          // Allow other tasks to run without blocking
          yield();
        }
        nmeaTcpBuffer = "";
      } else if (c >= 32 && c <= 126) {
        // Only accept printable ASCII characters
        if (nmeaTcpBuffer.length() < 120) { // NMEA max length is typically 82 chars
          nmeaTcpBuffer += c;
        } else {
          // Buffer overflow protection - reset
          nmeaTcpBuffer = "";
          Serial.println("NMEA TCP Server: Buffer overflow, resetting");
        }
      }
    }
  }

  // Clean up disconnected clients
  if (nmeaTcpClient && !nmeaTcpClient.connected()) {
    Serial.println("NMEA TCP Server: Client disconnected");
    nmeaTcpClient.stop();
    nmeaTcpBuffer = "";
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 SignalK Server ===\n");
  Serial.println("Firmware compiled with NMEA TCP server support");
  Serial.println("Ready to receive NMEA data on port 10110\n");

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
      if (obj.containsKey("anchor")) {
        JsonObject anchor = obj["anchor"];
        if (anchor.containsKey("lat") && anchor.containsKey("lon")) {
          geofence.anchorLat = anchor["lat"];
          geofence.anchorLon = anchor["lon"];
          geofence.anchorTimestamp = millis();
          Serial.printf("Restored anchor: %.6f, %.6f\n", geofence.anchorLat, geofence.anchorLon);
        }
        if (anchor.containsKey("radius")) {
          geofence.radius = anchor["radius"];
          Serial.printf("Restored radius: %.0f m\n", geofence.radius);
        }
        if (anchor.containsKey("enabled")) {
          geofence.enabled = anchor["enabled"];
          Serial.printf("Restored geofence enabled: %s\n", geofence.enabled ? "true" : "false");
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
          depthAlarm.enabled = depth["alarm"];
          Serial.printf("Restored depth alarm: %s\n", depthAlarm.enabled ? "true" : "false");
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
          windAlarm.enabled = wind["alarm"];
          Serial.printf("Restored wind alarm: %s\n", windAlarm.enabled ? "true" : "false");
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

  // NMEA UART
  Serial.println("Starting NMEA UART...");
  Serial1.begin(NMEA_BAUD, SERIAL_8N1, NMEA_RX, NMEA_TX);
  Serial.println("NMEA0183 UART started on pins RX:" + String(NMEA_RX) + " TX:" + String(NMEA_TX));

  // WebSocket setup
  Serial.println("Setting up WebSocket...");
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  Serial.println("WebSocket setup complete");

  // Start NMEA TCP server for receiving mock data
  Serial.println("Starting NMEA TCP server...");
  nmeaTcpServer.begin();
  Serial.println("NMEA TCP Server started on port 10110");

  // HTTP Routes

  // Root UI
  server.on("/", HTTP_GET, handleRoot);

  // TCP Configuration page
  server.on("/config", HTTP_GET, handleConfig);

  // Admin token management page
  server.on("/admin", HTTP_GET, handleAdmin);

  // Admin API endpoints
  server.on("/api/admin/tokens", HTTP_GET, handleGetAdminTokens);

  // TCP Configuration API
  server.on("/api/tcp/config", HTTP_GET, handleGetTcpConfig);
  server.on("/api/tcp/config", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL, handleSetTcpConfig);

  // Expo Push Notification API
  server.on("/plugins/signalk-node-red/redApi/register-expo-token", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL, handleRegisterExpoToken);

  // Test endpoint to verify server is reachable
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest* req) {
    Serial.println("=== TEST ENDPOINT HIT ===");
    Serial.printf("Client IP: %s\n", req->client()->remoteIP().toString().c_str());
    req->send(200, "text/plain", "ESP32 SignalK Server is running on port 3000!");
  });

  server.on("/test", HTTP_POST, [](AsyncWebServerRequest* req) {
    Serial.println("=== TEST POST ENDPOINT HIT ===");
    Serial.printf("Client IP: %s\n", req->client()->remoteIP().toString().c_str());
    req->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"POST received\"}");
  });

  // ===== SignalK Routes (IMPORTANT: Register most specific routes FIRST!) =====

  // Access request polling by requestId - MUST be before /signalk/v1/access/requests
  #ifdef ASYNCWEBSERVER_REGEX
  server.on("^\\/signalk\\/v1\\/access\\/requests\\/(.+)$", HTTP_GET, handleGetAccessRequestById);
  #endif

  // Access requests (auto-approve for SensESP compatibility)
  server.on("/signalk/v1/access/requests", HTTP_GET, handleGetAccessRequests);
  server.on("/signalk/v1/access/requests", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL, handleAccessRequest);

  // Token validation (always valid)
  server.on("/signalk/v1/auth/validate", HTTP_GET, handleAuthValidate);

  // API root
  server.on("/signalk/v1/api", HTTP_GET, handleAPIRoot);
  server.on("/signalk/v1/api/", HTTP_GET, handleAPIRoot);

  // SignalK discovery - MUST be registered LAST among /signalk routes
  server.on("/signalk", HTTP_GET, handleSignalKRoot);

  // Vessels
  server.on("/signalk/v1/api/vessels/self", HTTP_GET, handleVesselsSelf);
  server.on("/signalk/v1/api/vessels/self/", HTTP_GET, handleVesselsSelf);

#ifdef ASYNCWEBSERVER_REGEX
  // Dynamic path handlers - using regex patterns (requires heavy <regex> support)
  // Note: ESPAsyncWebServer regex support can be unreliable, so we also use rewrite rules
  server.on("^\\/signalk\\/v1\\/api\\/vessels\\/self\\/(.+)$", HTTP_GET, handleGetPath);
  server.on("^\\/signalk\\/v1\\/api\\/vessels\\/self\\/(.+)$", HTTP_PUT,
    [](AsyncWebServerRequest* req) {}, NULL, handlePutPath);
#else
  Serial.println("Regex routing disabled - relying on onNotFound fallback for vessels/self paths");
#endif

  // Fallback: Use onNotFound handler but check for vessels/self paths
  // This will be our backup if regex doesn't work

  // CORS headers
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

  // Handle requests with bodies (POST, PUT)
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String url = request->url();

    Serial.println("\n=== INCOMING REQUEST (onRequestBody) ===");
    Serial.printf("URL: %s\n", url.c_str());
    Serial.printf("Method: ");
    switch (request->method()) {
      case HTTP_GET: Serial.println("GET"); break;
      case HTTP_POST: Serial.println("POST"); break;
      case HTTP_PUT: Serial.println("PUT"); break;
      case HTTP_DELETE: Serial.println("DELETE"); break;
      case HTTP_OPTIONS: Serial.println("OPTIONS"); break;
      default: Serial.println(request->method()); break;
    }
    Serial.printf("Body chunk: index=%d, len=%d, total=%d\n", index, len, total);
    Serial.println("========================================\n");

    // Handle POST requests to access/requests
    if (request->method() == HTTP_POST && url == "/signalk/v1/access/requests") {
      Serial.println("Calling handleAccessRequest from onRequestBody...");
      handleAccessRequest(request, data, len, index, total);
      Serial.println("handleAccessRequest completed");
    }
    // Handle PUT requests to vessels/self paths
    else if (request->method() == HTTP_PUT && url.startsWith("/signalk/v1/api/vessels/self/") && url.length() > 29) {
      Serial.println("Calling handlePutPath from onRequestBody...");
      handlePutPath(request, data, len, index, total);
      Serial.println("handlePutPath completed");
    }
  });

  // Catch-all handler - route vessels/self paths if regex didn't match
  server.onNotFound([](AsyncWebServerRequest* req) {
    String url = req->url();

    // Debug: Log what we're checking
    Serial.printf("[DEBUG] onNotFound: method=%d, url=%s\n", req->method(), url.c_str());
    Serial.printf("[DEBUG] HTTP_POST value=%d\n", HTTP_POST);
    Serial.printf("[DEBUG] Checking admin API: POST=%d, starts=%d\n",
                  (req->method() == HTTP_POST), url.startsWith("/api/admin/"));

    // Check if this is an admin API POST request
    // Use integer comparison instead of enum comparison
    if (req->method() == 2 && url.startsWith("/api/admin/")) {
      Serial.println("=== ROUTING ADMIN API POST (regex fallback) ===");
      Serial.printf("URL: %s\n", url.c_str());
      handleAdminApiPost(req);
      return;
    }

    // Check if this is an access request polling path
    if (url.startsWith("/signalk/v1/access/requests/") && url.length() > 28) {
      Serial.println("=== ROUTING ACCESS REQUEST BY ID (regex fallback) ===");
      Serial.printf("URL: %s\n", url.c_str());
      if (req->method() == HTTP_GET) {
        handleGetAccessRequestById(req);
      }
      return;
    }

    // Check if this is a vessels/self path that should be handled
    if (url.startsWith("/signalk/v1/api/vessels/self/") && url.length() > 29) {
      Serial.println("=== ROUTING VESSELS/SELF PATH (regex fallback) ===");
      Serial.printf("URL: %s\n", url.c_str());
      Serial.printf("Method: %d (GET=1, PUT=8)\n", req->method());

      if (req->method() == HTTP_GET) {
        handleGetPath(req);
      } else if (req->method() == HTTP_PUT) {
        // PUT with body is handled in onRequestBody callback
        // Don't send 404 - body handler will respond after processing body
        Serial.println("PUT request detected - waiting for body handler to process");
      }
      // Don't send 404 for vessels/self paths
    } else {
      // Only send 404 if not a vessels/self path
      Serial.println("=== NOT FOUND / NOT IMPLEMENTED ===");
      Serial.print("Client IP: ");
      Serial.println(req->client()->remoteIP().toString());
      Serial.print("URL: ");
      Serial.println(req->url());
      Serial.print("Method: ");
      switch (req->method()) {
        case HTTP_GET: Serial.println("GET"); break;
        case HTTP_POST: Serial.println("POST"); break;
        case HTTP_PUT: Serial.println("PUT"); break;
        case HTTP_DELETE: Serial.println("DELETE"); break;
        case HTTP_OPTIONS: Serial.println("OPTIONS"); break;
        default: Serial.println(req->method()); break;
      }
      Serial.print("Content-Type: ");
      if (req->hasHeader("Content-Type")) {
        Serial.println(req->header("Content-Type"));
      } else {
        Serial.println("(none)");
      }
      Serial.println("===================================");
      req->send(404, "application/json", "{\"error\":\"Not found\"}");
    }
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
  wm.setConnectTimeout(30);           // Connection attempt timeout
  wm.setWiFiAutoReconnect(true);      // Auto reconnect to saved network
  wm.setSaveConfigCallback([]() {
    Serial.println("WiFi credentials saved!");
  });

  // Try to connect with saved credentials
  Serial.println("Attempting to connect with saved credentials...");

  // autoConnect will try saved credentials and start portal if it fails
  wm.autoConnect(AP_SSID, AP_PASSWORD);

  // Wait for connection attempt
  Serial.print("Connecting");
  int connectAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && connectAttempts < 20) {
    delay(500);
    Serial.print(".");
    connectAttempts++;
  }
  Serial.println();

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
String nmeaBuffer;
uint32_t lastWsCleanup = 0;
uint32_t lastStatusLog = 0;

void loop() {
  // Process WiFiManager (non-blocking)
  wm.process();

  // Log WiFi status periodically for debugging
  uint32_t now = millis();
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

  // Read NMEA sentences from Serial1
  while (Serial1.available()) {
    char c = Serial1.read();

    if (c == '\n' || c == '\r') {
      if (nmeaBuffer.length() > 6 && nmeaBuffer[0] == '$') {
        parseNMEASentence(nmeaBuffer);
      }
      nmeaBuffer = "";
    } else if (c >= 32 && c <= 126) {
      if (nmeaBuffer.length() < 120) {
        nmeaBuffer += c;
      }
    }
  }

  // TCP client connection and data processing
  connectToTcpServer();
  processTcpData();

  // NMEA TCP server for receiving mock data
  processNmeaTcpServer();

  // Broadcast WebSocket deltas
  if (ws.count() > 0) {
    broadcastDeltas();
  }

  // Cleanup WebSocket clients periodically
  if (now - lastWsCleanup > WS_CLEANUP_MS) {
    lastWsCleanup = now;
    ws.cleanupClients();
  }

  delay(1);
}
