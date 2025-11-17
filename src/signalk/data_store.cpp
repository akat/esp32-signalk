#include "data_store.h"
#include "globals.h"
#include "../utils/time_utils.h"
#include "../utils/conversions.h"
#include <ArduinoJson.h>
#include <cstring>
#include <math.h>

// Global data store - these are defined in main.cpp
extern std::map<String, PathValue> dataStore;
extern std::map<String, PathValue> lastSentValues;
extern std::map<String, String> notifications;

// Forward declaration for updateGeofence (will be in services/alarms)
extern void updateGeofence();
extern GeofenceConfig geofence;
extern DepthAlarmConfig depthAlarm;
extern WindAlarmConfig windAlarm;

static bool handleAnchorPartialUpdate(const String& path, bool isNumeric,
                                      double numericValue, const String& strValue,
                                      const String& source, const String& units,
                                      const String& description) {
  static const char* kAnchorPrefix = "navigation.anchor.akat.anchor.";
  if (!path.startsWith(kAnchorPrefix)) {
    return false;
  }

  String field = path.substring(strlen(kAnchorPrefix));
  if (field.length() == 0) {
    return false;
  }

  // Load existing anchor object if present
  DynamicJsonDocument doc(1024);
  auto existing = dataStore.find("navigation.anchor.akat");
  if (existing != dataStore.end() && existing->second.isJson && existing->second.jsonValue.length() > 0) {
    DeserializationError err = deserializeJson(doc, existing->second.jsonValue);
    if (err) {
      doc.clear();
    }
  }

  JsonObject anchorObj;
  if (doc.containsKey("anchor") && doc["anchor"].is<JsonObject>()) {
    anchorObj = doc["anchor"].as<JsonObject>();
  } else {
    anchorObj = doc.createNestedObject("anchor");
  }

  if (isNumeric) {
    anchorObj[field] = numericValue;
  } else {
    anchorObj[field] = strValue;
  }

  String json;
  serializeJson(doc, json);

  // Store the combined object back to navigation.anchor.akat
  setPathValueJson("navigation.anchor.akat", json, source, units,
                   description.length() > 0 ? description : "Anchor configuration");
  return true;
}

static String buildCanonicalAnchorConfig() {
  DynamicJsonDocument canonical(512);
  JsonObject anchorOut = canonical.createNestedObject("anchor");
  anchorOut["enabled"] = geofence.enabled;
  anchorOut["radius"] = geofence.radius;
  if (!isnan(geofence.anchorLat) && !isnan(geofence.anchorLon)) {
    anchorOut["lat"] = geofence.anchorLat;
    anchorOut["lon"] = geofence.anchorLon;
  }

  JsonObject depthOut = canonical.createNestedObject("depth");
  depthOut["alarm"] = depthAlarm.enabled;
  depthOut["min_depth"] = depthAlarm.threshold;

  JsonObject windOut = canonical.createNestedObject("wind");
  windOut["alarm"] = windAlarm.enabled;
  windOut["max_speed"] = windAlarm.threshold;

  String result;
  serializeJson(canonical, result);
  return result;
}

static String normalizeAnchorConfig(const String& jsonValue) {
  DynamicJsonDocument incoming(1024);
  DeserializationError err = deserializeJson(incoming, jsonValue);
  if (err || !incoming.is<JsonObject>()) {
    Serial.println("Anchor config: invalid payload, storing raw data");
    return jsonValue;
  }

  JsonObject obj = incoming.as<JsonObject>();

  bool depthChanged = false;
  bool windChanged = false;
  bool anchorPosChanged = false;
  bool radiusChanged = false;

  bool depthSection = false;
  bool windSection = false;

  bool newDepthEnabled = depthAlarm.enabled;
  double newDepthThreshold = depthAlarm.threshold;

  bool newWindEnabled = windAlarm.enabled;
  double newWindThreshold = windAlarm.threshold;

  if (obj.containsKey("depth") && obj["depth"].is<JsonObject>()) {
    depthSection = true;
    JsonObject depth = obj["depth"];
    if (depth.containsKey("alarm")) {
      bool candidate = depth["alarm"];
      if (candidate != depthAlarm.enabled) {
        depthChanged = true;
      }
      newDepthEnabled = candidate;
    }
    if (depth.containsKey("min_depth")) {
      double candidate = depth["min_depth"];
      if (!isnan(candidate) && fabs(candidate - depthAlarm.threshold) > 0.01) {
        depthChanged = true;
      }
      newDepthThreshold = candidate;
    }
  }

  if (obj.containsKey("wind") && obj["wind"].is<JsonObject>()) {
    windSection = true;
    JsonObject windCfg = obj["wind"];
    if (windCfg.containsKey("alarm")) {
      bool candidate = windCfg["alarm"];
      if (candidate != windAlarm.enabled) {
        windChanged = true;
      }
      newWindEnabled = candidate;
    }
    if (windCfg.containsKey("max_speed")) {
      double candidate = windCfg["max_speed"];
      if (!isnan(candidate) && fabs(candidate - windAlarm.threshold) > 0.01) {
        windChanged = true;
      }
      newWindThreshold = candidate;
    }
  }

  bool anchorEnabledProvided = false;
  bool newAnchorEnabled = geofence.enabled;

  if (obj.containsKey("anchor") && obj["anchor"].is<JsonObject>()) {
    JsonObject anchor = obj["anchor"];
    if (anchor.containsKey("lat") && anchor.containsKey("lon")) {
      double newLat = anchor["lat"];
      double newLon = anchor["lon"];
      if (fabs(newLat - geofence.anchorLat) > 0.00001 || fabs(newLon - geofence.anchorLon) > 0.00001) {
        anchorPosChanged = true;
      }
      geofence.anchorLat = newLat;
      geofence.anchorLon = newLon;
      geofence.anchorTimestamp = millis();
      Serial.printf("Anchor config: anchor set to %.6f, %.6f\n", geofence.anchorLat, geofence.anchorLon);
    }
    if (anchor.containsKey("radius")) {
      double newRadius = anchor["radius"];
      if (!isnan(newRadius) && fabs(newRadius - geofence.radius) > 0.1) {
        radiusChanged = true;
      }
      geofence.radius = newRadius;
      Serial.printf("Anchor config: radius %.0f m\n", geofence.radius);
    }
    if (anchor.containsKey("enabled")) {
      newAnchorEnabled = anchor["enabled"];
      anchorEnabledProvided = true;
    }
  }

  if (depthSection) {
    depthAlarm.threshold = newDepthThreshold;
    depthAlarm.enabled = newDepthEnabled;
    Serial.printf("Anchor config: Depth alarm %s, threshold %.1f m\n",
                  depthAlarm.enabled ? "ENABLED" : "DISABLED", depthAlarm.threshold);
  }

  if (windSection) {
    windAlarm.threshold = newWindThreshold;
    windAlarm.enabled = newWindEnabled;
    Serial.printf("Anchor config: Wind alarm %s, threshold %.1f kn\n",
                  windAlarm.enabled ? "ENABLED" : "DISABLED", windAlarm.threshold);
  }

  if (anchorEnabledProvided) {
    bool geofenceIntent = anchorPosChanged || radiusChanged || (!depthChanged && !windChanged);
    if (geofenceIntent) {
      if (geofence.enabled != newAnchorEnabled) {
        geofence.enabled = newAnchorEnabled;
        Serial.printf("Anchor config: Geofence enabled -> %s\n", geofence.enabled ? "true" : "false");
      }
    } else {
      Serial.println("Anchor config: Ignoring geofence.enabled because depth/wind changed");
    }
  }

  return buildCanonicalAnchorConfig();
}

void setPathValue(const String& path, double value, const String& source,
                  const String& units, const String& description) {
  if (handleAnchorPartialUpdate(path, true, value, "", source, units, description)) {
    return;
  }

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
}

void setPathValue(const String& path, const String& value, const String& source,
                  const String& units, const String& description) {
  if (handleAnchorPartialUpdate(path, false, 0.0, value, source, units, description)) {
    return;
  }

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
}

void setPathValueJson(const String& path, const String& jsonValue, const String& source,
                      const String& units, const String& description) {
  String normalized = jsonValue;
  if (path == "navigation.anchor.akat") {
    normalized = normalizeAnchorConfig(jsonValue);
  }

  PathValue& pv = dataStore[path];
  pv.isNumeric = false;
  pv.isJson = true;
  pv.jsonValue = normalized;
  pv.timestamp = iso8601Now();
  pv.source = source;
  pv.units = units;
  pv.description = description;
  pv.changed = true;

  // Persist important configuration paths to flash
  if (path == "navigation.anchor.akat") {
    Serial.printf("Attempting to persist %d bytes to flash\n", normalized.length());

    if (normalized.length() > 1900) {
      Serial.println("WARNING: JSON too large for NVS, truncating may occur");
    }

    prefs.begin("signalk", false);
    size_t written = prefs.putString("anchor.akat", normalized);

    // Verify it was written
    String readBack = prefs.getString("anchor.akat", "");
    prefs.end();

    if (written > 0 && readBack == normalized) {
      Serial.printf("SUCCESS: Persisted %d bytes to flash and verified\n", written);
    } else {
      Serial.printf("ERROR: Failed to persist (wrote %d bytes, readback length %d)\n", written, readBack.length());
    }
  }
}

void updateNavigationPosition(double lat, double lon, const String& source) {
  if (isnan(lat) || isnan(lon)) {
    Serial.println("Position update rejected - NaN values");
    return;
  }

  DynamicJsonDocument doc(128);
  doc["latitude"] = lat;
  doc["longitude"] = lon;

  String json;
  serializeJson(doc, json);

  setPathValueJson("navigation.position", json, source, "", "Vessel position");

  // Trigger geofence monitoring
  updateGeofence();
}

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
