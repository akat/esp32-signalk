#ifndef SIGNALK_GLOBALS_H
#define SIGNALK_GLOBALS_H

#include <Arduino.h>
#include <Preferences.h>
#include <map>
#include <vector>
#include "../types.h"

// Forward declarations for external dependencies
void updateGeofence();
void updateDepthAlarm(double depth);
void updateWindAlarm(double windSpeedMS);

// ====== PREFERENCES ======
extern Preferences prefs;

// ====== SIGNALK DATA ======
extern std::map<String, PathValue> dataStore;
extern std::map<String, PathValue> lastSentValues;
extern std::map<String, String> notifications;

// ====== GPS DATA ======
// Struct definitions are in types.h
extern GPSData gpsData;

// ====== GEOFENCE & ALARMS ======
// Struct definitions are in types.h
extern GeofenceConfig geofence;
extern DepthAlarmConfig depthAlarm;
extern WindAlarmConfig windAlarm;

#endif // SIGNALK_GLOBALS_H
