#ifndef SERVICES_EXPO_PUSH_H
#define SERVICES_EXPO_PUSH_H

#include <Arduino.h>

// Expo push notification constants
extern unsigned long lastPushNotification;
extern const unsigned long PUSH_NOTIFICATION_COOLDOWN;

// Functions
void sendExpoPushNotification(const String& title, const String& body,
                               const String& alarmType = "", const String& data = "");

#endif // SERVICES_EXPO_PUSH_H
