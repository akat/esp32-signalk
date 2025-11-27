#include "alarms.h"
#include "expo_push.h"
#include "../signalk/data_store.h"
#include "../utils/conversions.h"
#include <N2kMessages.h> // For msToKnots

// External globals
extern GPSData gpsData;
extern GeofenceConfig geofence;
extern DepthAlarmConfig depthAlarm;
extern WindAlarmConfig windAlarm;

// Geofence monitoring
void updateGeofence() {
  if (!geofence.enabled) {
    if (geofence.alarmActive) {
      clearNotification("geofence.exit");
      geofence.alarmActive = false;
    }
    return;
  }

  // Need valid anchor position
  if (isnan(geofence.anchorLat) || isnan(geofence.anchorLon)) {
    return;
  }

  // Get current position from dataStore (respects source priority)
  double currentLat, currentLon;
  if (!getCurrentPosition(currentLat, currentLon)) {
    return;
  }

  // Calculate distance
  double distance = haversineDistance(currentLat, currentLon,
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
}

// Depth alarm monitoring
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

// Wind alarm monitoring
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
