/*
  ESP32 SignalK Server - Full Implementation
  Compatible with SignalK clients and apps
  Features:
  - WiFiManager for easy setup (like SenseESP)
  - Full SignalK REST API structure
  - WebSocket with subscribe/unsubscribe
  - Authentication with tokens
  - NMEA0183 parsing (GPS)
  - Metadata & units support
  - Delta compression
*/

#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <time.h>
#include <vector>
#include <map>
#include <set>

// ====== CONFIGURATION ======
#define NMEA_RX 16
#define NMEA_TX 17
#define NMEA_BAUD 4800

#define WS_DELTA_MIN_MS 100        // Minimum delta broadcast interval
#define WS_CLEANUP_MS 5000         // WebSocket cleanup interval
#define AUTH_TOKEN_LENGTH 32       // Token length

// ====== GLOBALS ======
AsyncWebServer server(80);
AsyncWebSocket ws("/signalk/v1/stream");
Preferences prefs;
WiFiManager wm;

// SignalK vessel UUID (persistent)
String vesselUUID;
String serverName = "ESP32-SignalK";

// Authentication tokens (stored in NVS)
std::map<String, String> authTokens; // token -> role ("admin", "readonly")

// ====== PATH STORAGE ======
struct PathValue {
  JsonVariant value;
  String strValue;      // For string storage
  double numValue;
  bool isNumeric;
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
  uint32_t minPeriod;
  String format; // "delta" or "full"
  uint32_t lastSend;
};

std::map<uint32_t, ClientSubscription> clientSubscriptions; // clientId -> subscription

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

// ====== UTILITY FUNCTIONS ======
String generateUUID() {
  char uuid[57];
  snprintf(uuid, sizeof(uuid), "urn:mrn:signalk:uuid:%08x-%04x-%04x-%04x-%012x",
    esp_random(), esp_random() & 0xFFFF, esp_random() & 0xFFFF,
    esp_random() & 0xFFFF, esp_random());
  return String(uuid);
}

String generateToken() {
  const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  String token = "";
  for(int i = 0; i < AUTH_TOKEN_LENGTH; i++) {
    token += chars[esp_random() % 62];
  }
  return token;
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
double degToRad(double deg) { return deg * M_PI / 180.0; }

// ====== PATH OPERATIONS ======
void setPathValue(const String& path, double value, const String& source = "nmea0183.GPS", 
                  const String& units = "", const String& description = "") {
  PathValue& pv = dataStore[path];
  pv.numValue = value;
  pv.isNumeric = true;
  pv.timestamp = iso8601Now();
  pv.source = source;
  pv.units = units;
  pv.description = description;
  pv.changed = true;
  
  // Check if value actually changed
  if (lastSentValues.count(path) > 0) {
    if (abs(lastSentValues[path].numValue - value) < 0.0001) {
      pv.changed = false;
    }
  }
}

void setPathValue(const String& path, const String& value, const String& source = "nmea0183.GPS",
                  const String& units = "", const String& description = "") {
  PathValue& pv = dataStore[path];
  pv.strValue = value;
  pv.isNumeric = false;
  pv.timestamp = iso8601Now();
  pv.source = source;
  pv.units = units;
  pv.description = description;
  pv.changed = true;
  
  if (lastSentValues.count(path) > 0) {
    if (lastSentValues[path].strValue == value) {
      pv.changed = false;
    }
  }
}

// ====== NMEA PARSING ======
double nmeaCoordToDec(const String& coord, const String& hemisphere) {
  if (coord.length() < 4) return NAN;
  
  int dotPos = coord.indexOf('.');
  if (dotPos < 0) return NAN;
  
  int degLen = (dotPos <= 3) ? 2 : 3; // 2 for lat, 3 for lon
  
  double degrees = coord.substring(0, degLen).toDouble();
  double minutes = coord.substring(degLen).toDouble();
  double decimal = degrees + (minutes / 60.0);
  
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
      
      setPathValue("navigation.position.latitude", lat, "nmea0183.GPS", "deg", "Latitude");
      setPathValue("navigation.position.longitude", lon, "nmea0183.GPS", "deg", "Longitude");
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
      
      setPathValue("navigation.position.latitude", lat, "nmea0183.GPS", "deg", "Latitude");
      setPathValue("navigation.position.longitude", lon, "nmea0183.GPS", "deg", "Longitude");
      setPathValue("navigation.gnss.satellitesInView", (double)sats, "nmea0183.GPS", "", "Satellites in view");
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
}

// ====== AUTHENTICATION ======
void loadAuthTokens() {
  prefs.begin("signalk", false);
  int count = prefs.getInt("token_count", 0);
  
  for (int i = 0; i < count; i++) {
    String tokenKey = "token_" + String(i);
    String roleKey = "role_" + String(i);
    
    String token = prefs.getString(tokenKey.c_str(), "");
    String role = prefs.getString(roleKey.c_str(), "");
    
    if (token.length() > 0) {
      authTokens[token] = role;
    }
  }
  
  // Create default admin token if none exists
  if (authTokens.empty()) {
    String defaultToken = generateToken();
    authTokens[defaultToken] = "admin";
    prefs.putString("token_0", defaultToken);
    prefs.putString("role_0", "admin");
    prefs.putInt("token_count", 1);
    Serial.println("\n=== DEFAULT ADMIN TOKEN ===");
    Serial.println(defaultToken);
    Serial.println("===========================\n");
  }
  
  prefs.end();
}

bool validateToken(const String& token, bool requireAdmin = false) {
  if (token.length() == 0) return false;
  
  auto it = authTokens.find(token);
  if (it == authTokens.end()) return false;
  
  if (requireAdmin && it->second != "admin") return false;
  
  return true;
}

String getTokenFromRequest(AsyncWebServerRequest* req) {
  // Check Authorization header (Bearer token)
  if (req->hasHeader("Authorization")) {
    String auth = req->getHeader("Authorization")->value();
    if (auth.startsWith("Bearer ")) {
      return auth.substring(7);
    }
  }
  
  // Check cookie
  if (req->hasHeader("Cookie")) {
    String cookies = req->getHeader("Cookie")->value();
    int pos = cookies.indexOf("JAUTHENTICATION=");
    if (pos >= 0) {
      int end = cookies.indexOf(';', pos);
      if (end < 0) end = cookies.length();
      return cookies.substring(pos + 16, end);
    }
  }
  
  return "";
}

// ====== WEBSOCKET DELTA BROADCAST ======
void broadcastDeltas() {
  static uint32_t lastBroadcast = 0;
  uint32_t now = millis();
  
  if (now - lastBroadcast < WS_DELTA_MIN_MS) return;
  lastBroadcast = now;
  
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
    
    if (kv.second.isNumeric) {
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
  
  // Send to all connected clients
  ws.textAll(output);
}

// ====== WEBSOCKET EVENT HANDLER ======
void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, data, len);
  
  if (error) {
    Serial.println("WS: Invalid JSON");
    return;
  }
  
  String context = doc["context"] | "";
  String subscribe = doc["subscribe"] | "";
  String unsubscribe = doc["unsubscribe"] | "";
  
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
    
    sub.minPeriod = doc["minPeriod"] | 1000;
    sub.format = doc["format"] | "delta";
    
    // Send hello message
    DynamicJsonDocument hello(512);
    hello["self"] = "vessels." + vesselUUID;
    hello["version"] = "1.0.0";
    hello["timestamp"] = iso8601Now();
    
    String output;
    serializeJson(hello, output);
    client->text(output);
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
      Serial.printf("WS: Client #%u connected\n", client->id());
      break;
      
    case WS_EVT_DISCONNECT:
      Serial.printf("WS: Client #%u disconnected\n", client->id());
      clientSubscriptions.erase(client->id());
      break;
      
    case WS_EVT_DATA:
      handleWebSocketMessage(client, data, len);
      break;
      
    default:
      break;
  }
}

// ====== HTTP HANDLERS ======

// GET /signalk - Server info
void handleSignalKRoot(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(1024);
  
  JsonObject endpoints = doc.createNestedObject("endpoints");
  JsonObject v1 = endpoints.createNestedObject("v1");
  
  v1["version"] = "1.0.0";
  v1["signalk-http"] = "http://" + WiFi.localIP().toString() + "/signalk/v1/api/";
  v1["signalk-ws"] = "ws://" + WiFi.localIP().toString() + "/signalk/v1/stream";
  
  JsonObject server = doc.createNestedObject("server");
  server["id"] = serverName;
  server["version"] = "1.0.0";
  
  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

// GET /signalk/v1/api/ - API root
void handleAPIRoot(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(512);
  doc["name"] = serverName;
  doc["version"] = "1.0.0";
  doc["self"] = "vessels." + vesselUUID;
  
  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

// GET /signalk/v1/api/vessels/self
void handleVesselsSelf(AsyncWebServerRequest* req) {
  DynamicJsonDocument doc(4096);
  
  doc["uuid"] = vesselUUID;
  doc["name"] = serverName;
  
  // Navigation data
  if (dataStore.size() > 0) {
    JsonObject nav = doc.createNestedObject("navigation");
    
    for (auto& kv : dataStore) {
      String path = kv.first;
      if (!path.startsWith("navigation.")) continue;
      
      String subPath = path.substring(11); // Remove "navigation."
      
      JsonObject value = nav.createNestedObject(subPath);
      value["timestamp"] = kv.second.timestamp;
      
      if (kv.second.isNumeric) {
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
  
  if (pv.isNumeric) {
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

// PUT /signalk/v1/api/vessels/self/* - Update path (requires admin)
void handlePutPath(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  String token = getTokenFromRequest(req);
  
  if (!validateToken(token, true)) {
    req->send(401, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":401,\"message\":\"Unauthorized\"}");
    return;
  }
  
  String path = req->url().substring(String("/signalk/v1/api/vessels/self/").length());
  path.replace("/", ".");
  
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, data, len);
  
  if (error) {
    req->send(400, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"Invalid JSON\"}");
    return;
  }
  
  JsonVariant value = doc["value"];
  
  if (value.is<double>() || value.is<int>()) {
    setPathValue(path, value.as<double>(), "manual", "", "Manually set");
  } else if (value.is<const char*>()) {
    setPathValue(path, String(value.as<const char*>()), "manual", "", "Manually set");
  } else {
    req->send(400, "application/json", "{\"state\":\"COMPLETED\",\"statusCode\":400,\"message\":\"Invalid value type\"}");
    return;
  }
  
  DynamicJsonDocument response(256);
  response["state"] = "COMPLETED";
  response["statusCode"] = 200;
  
  String output;
  serializeJson(response, output);
  req->send(200, "application/json", output);
}

// GET /admin/tokens - List tokens (admin only)
void handleListTokens(AsyncWebServerRequest* req) {
  String token = getTokenFromRequest(req);
  
  if (!validateToken(token, true)) {
    req->send(401, "text/plain", "Unauthorized");
    return;
  }
  
  DynamicJsonDocument doc(2048);
  JsonArray tokens = doc.to<JsonArray>();
  
  for (auto& kv : authTokens) {
    JsonObject t = tokens.createNestedObject();
    t["token"] = kv.first;
    t["role"] = kv.second;
  }
  
  String output;
  serializeJson(doc, output);
  req->send(200, "application/json", output);
}

// POST /admin/tokens - Create new token (admin only)
void handleCreateToken(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  String token = getTokenFromRequest(req);
  
  if (!validateToken(token, true)) {
    req->send(401, "text/plain", "Unauthorized");
    return;
  }
  
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, data, len);
  
  String role = doc["role"] | "readonly";
  String newToken = generateToken();
  
  authTokens[newToken] = role;
  
  // Save to NVS
  prefs.begin("signalk", false);
  int count = prefs.getInt("token_count", 0);
  prefs.putString(("token_" + String(count)).c_str(), newToken);
  prefs.putString(("role_" + String(count)).c_str(), role);
  prefs.putInt("token_count", count + 1);
  prefs.end();
  
  DynamicJsonDocument response(256);
  response["token"] = newToken;
  response["role"] = role;
  
  String output;
  serializeJson(response, output);
  req->send(200, "application/json", output);
}

// Simple web UI
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
  </style>
</head>
<body>
  <div class="container">
    <h1>🚢 SignalK Server - ESP32</h1>
    
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
      
      // Subscribe to all paths
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
        
        // Handle hello message
        if (msg.self) {
          document.getElementById('vessel-uuid').textContent = msg.self;
        }
        
        // Handle delta updates
        if (msg.updates) {
          msg.updates.forEach(update => {
            if (update.values) {
              update.values.forEach(item => {
                const path = item.path;
                let value = item.value;
                
                // Format value
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
      
      // Sort paths alphabetically
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

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 SignalK Server ===\n");
  
  // Load or generate vessel UUID
  prefs.begin("signalk", false);
  vesselUUID = prefs.getString("vessel_uuid", "");
  if (vesselUUID.length() == 0) {
    vesselUUID = generateUUID();
    prefs.putString("vessel_uuid", vesselUUID);
    Serial.println("Generated new vessel UUID: " + vesselUUID);
  }
  serverName = prefs.getString("server_name", "ESP32-SignalK");
  prefs.end();
  
  // Load authentication tokens
  loadAuthTokens();

  // Print admin token for convenience
  if (!authTokens.empty()) {
    for (auto& kv : authTokens) {
      if (kv.second == "admin") {
        Serial.println("\n=== CURRENT ADMIN TOKEN ===");
        Serial.println(kv.first);
        Serial.println("===========================\n");
        break;
      }
    }
  }
  
  // WiFiManager setup (like SenseESP)
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalTimeout(180); // 3 minutes timeout
  wm.setAPClientCheck(true);
  wm.setBreakAfterConfig(true);
  
  // Custom parameters
  WiFiManagerParameter custom_name("name", "Server Name", serverName.c_str(), 40);
  wm.addParameter(&custom_name);
  
  Serial.println("Starting WiFiManager...");
  Serial.println("If no saved WiFi, AP will start as 'SignalK-Setup'");
  
  bool res = wm.autoConnect("SignalK-Setup", "signalk123");
  
  if (!res) {
    Serial.println("Failed to connect, restarting...");
    delay(3000);
    ESP.restart();
  }
  
  // Save custom parameters
  String newName = custom_name.getValue();
  if (newName.length() > 0 && newName != serverName) {
    serverName = newName;
    prefs.begin("signalk", false);
    prefs.putString("server_name", serverName);
    prefs.end();
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.println("IP Address: " + WiFi.localIP().toString());
  Serial.println("SignalK API: http://" + WiFi.localIP().toString() + "/signalk/v1/api/");
  Serial.println("WebSocket: ws://" + WiFi.localIP().toString() + "/signalk/v1/stream");
  
  // NTP Time sync
  configTime(0, 0, "pool.ntp.org", "time.ntp.org");
  Serial.println("Syncing time with NTP...");
  
  // Wait for time sync
  int timeRetries = 0;
  while (time(nullptr) < 100000 && timeRetries < 20) {
    delay(500);
    Serial.print(".");
    timeRetries++;
  }
  Serial.println(time(nullptr) > 100000 ? " OK" : " FAILED");
  
  // NMEA UART
  Serial1.begin(NMEA_BAUD, SERIAL_8N1, NMEA_RX, NMEA_TX);
  Serial.println("NMEA0183 UART started on pins RX:" + String(NMEA_RX) + " TX:" + String(NMEA_TX));
  
  // WebSocket setup
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  
  // HTTP Routes
  
  // Root UI
  server.on("/", HTTP_GET, handleRoot);
  
  // SignalK discovery
  server.on("/signalk", HTTP_GET, handleSignalKRoot);
  
  // API root
  server.on("/signalk/v1/api", HTTP_GET, handleAPIRoot);
  server.on("/signalk/v1/api/", HTTP_GET, handleAPIRoot);
  
  // Vessels
  server.on("/signalk/v1/api/vessels/self", HTTP_GET, handleVesselsSelf);
  server.on("/signalk/v1/api/vessels/self/", HTTP_GET, handleVesselsSelf);
  
  // Dynamic path handlers
  server.on("^\\/signalk\\/v1\\/api\\/vessels\\/self\\/(.+)$", HTTP_GET, handleGetPath);
  server.on("^\\/signalk\\/v1\\/api\\/vessels\\/self\\/(.+)$", HTTP_PUT, 
    [](AsyncWebServerRequest* req) {}, NULL, handlePutPath);
  
  // Admin endpoints
  server.on("/admin/tokens", HTTP_GET, handleListTokens);
  server.on("/admin/tokens", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL, handleCreateToken);
  
  // CORS headers for all routes
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key");
  
  server.begin();
  Serial.println("\nHTTP Server started");
  Serial.println("\n=== System Ready ===\n");
}

// ====== MAIN LOOP ======
String nmeaBuffer;
uint32_t lastWsCleanup = 0;

void loop() {
  // Read NMEA sentences from Serial1
  while (Serial1.available()) {
    char c = Serial1.read();

    if (c == '\n' || c == '\r') {
      if (nmeaBuffer.length() > 6 && nmeaBuffer[0] == '$') {
        parseNMEASentence(nmeaBuffer);
      }
      nmeaBuffer = "";
    } else if (c >= 32 && c <= 126) { // Printable ASCII
      if (nmeaBuffer.length() < 120) {
        nmeaBuffer += c;
      }
    }
  }
  
  // Broadcast WebSocket deltas
  if (ws.count() > 0) {
    broadcastDeltas();
  }
  
  // Cleanup WebSocket clients periodically
  uint32_t now = millis();
  if (now - lastWsCleanup > WS_CLEANUP_MS) {
    lastWsCleanup = now;
    ws.cleanupClients();
  }
  
  // Small delay to prevent watchdog issues
  delay(1);
}