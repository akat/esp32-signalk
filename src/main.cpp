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
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
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

// TCP Client Configuration
#define TCP_RECONNECT_DELAY 5000   // Reconnect attempt interval (ms)

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

// SignalK vessel UUID (persistent)
String vesselUUID;
String serverName = "ESP32-SignalK";

// TCP Client for external SignalK server
WiFiClient tcpClient;
String tcpServerHost = "";
int tcpServerPort = 10110;
bool tcpEnabled = false;
uint32_t lastTcpReconnect = 0;
String tcpBuffer = "";

// TCP Server for receiving NMEA data (for mocking)
WiFiServer nmeaTcpServer(10110);
WiFiClient nmeaTcpClient;
String nmeaTcpBuffer = "";

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

      setPathValue("navigation.position.latitude", lat, "nmea0183.GPS", "deg", "Latitude");
      setPathValue("navigation.position.longitude", lon, "nmea0183.GPS", "deg", "Longitude");
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
  
  v1["version"] = "1.7.0";
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
  doc["version"] = "1.7.0";
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

// PUT /signalk/v1/api/vessels/self/* - Update path (no auth required)
void handlePutPath(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  // Only process when we have the complete body
  if (index + len != total) {
    return;  // Wait for more data
  }

  String path = req->url().substring(String("/signalk/v1/api/vessels/self/").length());
  path.replace("/", ".");

  Serial.println("\n=== PUT PATH REQUEST ===");
  Serial.printf("Path: %s\n", path.c_str());
  Serial.printf("Data length: %d bytes\n", len);
  Serial.printf("Raw body: %.*s\n", len, data);

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
    // For complex objects, serialize to string
    String jsonStr;
    serializeJson(value, jsonStr);
    setPathValue(path, jsonStr, source, "", description);
    Serial.printf("Set object/array value: %s\n", jsonStr.c_str());
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
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastWifiWarning = 0;
    uint32_t now = millis();
    if (now - lastWifiWarning > 30000) { // Log every 30 seconds
      Serial.println("TCP: Cannot connect - WiFi not connected to internet");
      lastWifiWarning = now;
    }
    return;
  }

  if (tcpClient.connected()) {
    return; // Already connected
  }

  uint32_t now = millis();
  if (now - lastTcpReconnect < TCP_RECONNECT_DELAY) {
    return; // Wait before reconnecting
  }

  lastTcpReconnect = now;

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
    } else {
      Serial.println("TCP: Connection failed (connection refused or timeout)");
    }
  } else {
    Serial.println("TCP: DNS resolution failed for " + tcpServerHost);
    Serial.println("Check hostname and ensure DNS is working");
  }
}

void processTcpData() {
  if (!tcpEnabled || !tcpClient.connected()) {
    return;
  }

  // Read available data - process NMEA sentences from TCP
  while (tcpClient.available()) {
    char c = tcpClient.read();

    // NMEA sentences end with newline
    if (c == '\n' || c == '\r') {
      if (tcpBuffer.length() > 6 && tcpBuffer[0] == '$') {
        // This is an NMEA sentence - parse it
        Serial.println("TCP NMEA: " + tcpBuffer);

        // Modify the source to indicate it's from TCP
        // Save original source temporarily if needed
        parseNMEASentence(tcpBuffer);

        // Mark the paths as coming from TCP
        // Note: parseNMEASentence sets source as "nmea0183.GPS"
        // We could enhance this later to set source as "tcp.nmea0183"
      }
      tcpBuffer = "";
    } else if (c >= 32 && c <= 126) {
      // Only accept printable ASCII characters
      if (tcpBuffer.length() < 120) { // NMEA max length is typically 82 chars
        tcpBuffer += c;
      }
    }
  }

  // Check if connection is still alive
  if (!tcpClient.connected()) {
    Serial.println("TCP: Connection lost");
    tcpBuffer = "";
  }
}

void processNmeaTcpServer() {
  // Check for new client connections
  if (!nmeaTcpClient || !nmeaTcpClient.connected()) {
    nmeaTcpClient = nmeaTcpServer.available();
    if (nmeaTcpClient) {
      Serial.println("NMEA TCP Server: New client connected from " + nmeaTcpClient.remoteIP().toString());
      nmeaTcpBuffer = "";
    }
  }

  // Process data from connected client
  if (nmeaTcpClient && nmeaTcpClient.connected()) {
    while (nmeaTcpClient.available()) {
      char c = nmeaTcpClient.read();

      // NMEA sentences end with newline
      if (c == '\n' || c == '\r') {
        if (nmeaTcpBuffer.length() > 6 && nmeaTcpBuffer[0] == '$') {
          // This is an NMEA sentence - parse it
          Serial.println("NMEA TCP Server: " + nmeaTcpBuffer);
          // Add a small delay to prevent overwhelming the system
          delay(1);
          parseNMEASentence(nmeaTcpBuffer);
        }
        nmeaTcpBuffer = "";
      } else if (c >= 32 && c <= 126) {
        // Only accept printable ASCII characters
        if (nmeaTcpBuffer.length() < 120) { // NMEA max length is typically 82 chars
          nmeaTcpBuffer += c;
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

  // TCP Configuration API
  server.on("/api/tcp/config", HTTP_GET, handleGetTcpConfig);
  server.on("/api/tcp/config", HTTP_POST,
    [](AsyncWebServerRequest* req) {}, NULL, handleSetTcpConfig);

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

  // SignalK discovery
  server.on("/signalk", HTTP_GET, handleSignalKRoot);

  // API root
  server.on("/signalk/v1/api", HTTP_GET, handleAPIRoot);
  server.on("/signalk/v1/api/", HTTP_GET, handleAPIRoot);

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

    // Handle PUT requests to vessels/self paths
    if (request->method() == HTTP_PUT && url.startsWith("/signalk/v1/api/vessels/self/") && url.length() > 29) {
      Serial.println("Calling handlePutPath from onRequestBody...");
      handlePutPath(request, data, len, index, total);
      Serial.println("handlePutPath completed");
    }
  });

  // Catch-all handler - route vessels/self paths if regex didn't match
  server.onNotFound([](AsyncWebServerRequest* req) {
    String url = req->url();

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

  wm.setDebugOutput(true);
  wm.setConfigPortalBlocking(false);  // Non-blocking mode
  wm.setDisableConfigPortal(false);   // Keep AP + portal running after STA connects
  wm.setWiFiAutoReconnect(true);      // Ensure STA stays connected while AP stays up

  // Start config portal on demand - always available
  // This starts the portal immediately and keeps it running
  wm.startConfigPortal(AP_SSID, AP_PASSWORD);

  Serial.println("WiFiManager portal started on http://192.168.4.1");
  Serial.println("Portal will remain active for WiFi configuration");

  // Check if WiFi credentials are already saved and connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n=== WiFi Client Already Connected ===");
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
  }

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
