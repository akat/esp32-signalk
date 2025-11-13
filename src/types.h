#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include <vector>
#include <map>
#include <set>
#include <ArduinoJson.h>

// ====== ACCESS REQUEST SYSTEM ======
struct AccessRequest {
  String requestId;
  String clientId;
  String description;
  std::vector<String> permissions;
  String state; // "PENDING", "APPROVED", "DENIED"
  String token; // Generated when approved
  unsigned long timestamp;
};

// Access request data with extended fields for SignalK protocol
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

// ====== TCP CLIENT STATE ======
enum TcpClientState {
  TCP_DISCONNECTED,
  TCP_CONNECTING,
  TCP_CONNECTED,
  TCP_ERROR
};

// ====== PATH STORAGE ======
struct PathValue {
  JsonVariant value;
  String strValue;      // For string storage
  double numValue;
  bool isNumeric = false;
  bool isJson = false;
  String jsonValue;     // For JSON storage
  String timestamp;     // ISO8601
  String source;        // e.g., "nmea0183.GPS"

  // Metadata
  String units;
  String description;

  bool changed;         // For delta compression
};

// ====== WEBSOCKET SUBSCRIPTIONS ======
struct ClientSubscription {
  std::set<String> paths;
  uint32_t minPeriod;
  String format; // "delta" or "full"
  uint32_t lastSend;
};

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
};

// ====== GEOFENCE & ALARMS ======
struct GeofenceConfig {
  bool enabled = false;
  double anchorLat = NAN;
  double anchorLon = NAN;
  double radius = 100.0;  // meters
  uint32_t anchorTimestamp = 0;
  bool alarmActive = false;
  double lastDistance = NAN;
};

struct DepthAlarmConfig {
  bool enabled = false;
  double threshold = 2.0;  // meters
  bool alarmActive = false;
  double lastDepth = NAN;
  uint32_t lastSampleTime = 0;
};

struct WindAlarmConfig {
  bool enabled = false;
  double threshold = 20.0;  // knots
  bool alarmActive = false;
  double lastWind = NAN;
  uint32_t lastSampleTime = 0;
};

// ====== TOKEN STORAGE ======
struct ApprovedToken {
  String token;
  String clientId;
  String description;
  String permissions;
  unsigned long approvedAt;
};

#endif
