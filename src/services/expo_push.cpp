#include "expo_push.h"
#include "storage.h"
#include <WiFiClient.h>
#include <ArduinoJson.h>

// Expo push notification state (extern - defined in main.cpp)
extern unsigned long lastPushNotification;
const unsigned long PUSH_NOTIFICATION_COOLDOWN = 30000;  // 30 seconds between notification batches
static const char* PUSH_PROXY_HOST = "pushit.digitalspot.gr";
static const uint16_t PUSH_PROXY_PORT = 80;
static const char* PUSH_PROXY_PATH = "/push";

// Queue for serialized push notification sending
struct PushNotificationQueueItem {
  String token;
  String title;
  String body;
  String alarmType;
  String data;
  unsigned long sendAfter;  // millis timestamp when this should be sent
};

std::vector<PushNotificationQueueItem> pushQueue;
unsigned long lastQueueProcessTime = 0;
const unsigned long SERIAL_SEND_INTERVAL = 3000;  // 3 seconds between individual sends

void sendExpoPushNotification(const String& title, const String& body,
                               const String& alarmType, const String& data) {
  unsigned long now = millis();

  // Rate limiting check - only applies to adding new notifications to queue
  if (now - lastPushNotification < PUSH_NOTIFICATION_COOLDOWN) {
    // Don't spam the serial output
    static unsigned long lastRateLimitLog = 0;
    if (now - lastRateLimitLog > 5000) {  // Log only every 5 seconds
      Serial.println("Push notification rate limited");
      lastRateLimitLog = now;
    }
    return;
  }

  if (expoTokens.empty()) {
    Serial.println("No Expo tokens registered");
    return;
  }

  lastPushNotification = now;

  // Add each token to the queue with staggered send times
  Serial.printf("Queueing push notifications for %d tokens (serial mode)\n", expoTokens.size());
  int tokenIndex = 0;
  for (const auto& token : expoTokens) {
    PushNotificationQueueItem item;
    item.token = token;
    item.title = title;
    item.body = body;
    item.alarmType = alarmType;
    item.data = data;
    item.sendAfter = now + (tokenIndex * SERIAL_SEND_INTERVAL);  // Stagger by 3 seconds each
    pushQueue.push_back(item);
    tokenIndex++;
  }
}

// Call this from main loop to process the queue
void processPushNotificationQueue() {
  if (pushQueue.empty()) {
    return;
  }

  unsigned long now = millis();

  // Process one item from the queue if it's time
  for (auto it = pushQueue.begin(); it != pushQueue.end(); ) {
    if (now >= it->sendAfter) {
      Serial.printf("Sending queued push to: %s\n", it->token.substring(0, 30).c_str());

      // Build JSON payload for push proxy
      DynamicJsonDocument doc(512);
      doc["to"] = it->token;
      doc["title"] = it->title;
      doc["body"] = it->body;
      doc["priority"] = "high";

      // Set sound and channelId based on alarm type
      if (it->alarmType == "geofence") {
        doc["sound"] = "geofence_alarm.wav";
        doc["channelId"] = "geofence-alarms";
      } else if (it->alarmType == "depth") {
        doc["sound"] = "geofence_alarm.wav";
        doc["channelId"] = "geofence-alarms";
      } else if (it->alarmType == "wind") {
        doc["sound"] = "geofence_alarm.wav";
        doc["channelId"] = "geofence-alarms";
      } else {
        doc["sound"] = "default";
      }

      if (it->data.length() > 0) {
        doc["data"] = it->data;
      }

      String payload;
      serializeJson(doc, payload);

      // Send HTTP POST to push proxy (HTTP)
      WiFiClient client;
      client.setTimeout(5000);

      if (client.connect(PUSH_PROXY_HOST, PUSH_PROXY_PORT)) {
        client.print("POST ");
        client.print(PUSH_PROXY_PATH);
        client.println(" HTTP/1.1");
        client.print("Host: ");
        client.println(PUSH_PROXY_HOST);
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
              Serial.printf("Push proxy response: %s\n", line.c_str());
            }
          }
        }
        client.stop();
      } else {
        Serial.println("Failed to connect to push proxy");
      }

      // Remove from queue and return (process only one per call)
      it = pushQueue.erase(it);
      return;
    } else {
      ++it;
    }
  }
}
