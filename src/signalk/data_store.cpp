#include "data_store.h"
#include "globals.h"
#include "../utils/time_utils.h"
#include "../utils/conversions.h"
#include <ArduinoJson.h>
#include <cstring>

// Global data store - these are defined in main.cpp
extern std::map<String, PathValue> dataStore;
extern std::map<String, PathValue> lastSentValues;
extern std::map<String, String> notifications;

// Forward declaration for updateGeofence (will be in services/alarms)
extern void updateGeofence();

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
