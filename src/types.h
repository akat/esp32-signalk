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

#endif
