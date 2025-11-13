#include "expo_push.h"
#include "storage.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Expo push notification state (extern - defined in main.cpp)
extern unsigned long lastPushNotification;
const unsigned long PUSH_NOTIFICATION_COOLDOWN = 10000;  // 10 seconds

void sendExpoPushNotification(const String& title, const String& body,
                               const String& alarmType, const String& data) {
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
