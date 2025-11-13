#ifndef SIGNALK_DATA_STORE_H
#define SIGNALK_DATA_STORE_H

#include <Arduino.h>
#include <map>
#include "../types.h"

// Global data store for SignalK paths
extern std::map<String, PathValue> dataStore;
extern std::map<String, PathValue> lastSentValues;
extern std::map<String, String> notifications;

// Path operations
void setPathValue(const String& path, double value, const String& source = "nmea0183.GPS",
                  const String& units = "", const String& description = "");

void setPathValue(const String& path, const String& value, const String& source = "nmea0183.GPS",
                  const String& units = "", const String& description = "");

void setPathValueJson(const String& path, const String& jsonValue, const String& source = "nmea0183.GPS",
                      const String& units = "", const String& description = "");

void updateNavigationPosition(double lat, double lon, const String& source = "nmea0183.GPS");

// Notification operations
void setNotification(const String& path, const String& state, const String& message);
void clearNotification(const String& path);

#endif // SIGNALK_DATA_STORE_H
