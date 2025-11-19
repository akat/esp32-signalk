#include "dyndns.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "../types.h"
#include "../utils/time_utils.h"

extern DynDnsConfig dynDnsConfig;

namespace {
  constexpr uint32_t kDynDnsIntervalMs = 15UL * 60UL * 1000UL;  // 15 minutes
  bool forceUpdate = false;
  uint32_t lastAttempt = 0;

  void setStatus(const String& message, bool success) {
    dynDnsConfig.lastResult = message;
    dynDnsConfig.lastSuccess = success;
    dynDnsConfig.lastUpdated = iso8601Now();
    dynDnsConfig.lastUpdateMs = millis();
  }

  bool credentialsReady() {
    if (dynDnsConfig.provider == "duckdns") {
      return dynDnsConfig.hostname.length() > 0 && dynDnsConfig.token.length() > 0;
    }
    return dynDnsConfig.hostname.length() > 0 &&
           dynDnsConfig.username.length() > 0 &&
           dynDnsConfig.password.length() > 0;
  }

  void performUpdate() {
    if (!dynDnsConfig.enabled) {
      forceUpdate = false;
      return;
    }

    if (!credentialsReady()) {
      setStatus("Missing DynDNS hostname or credentials", false);
      lastAttempt = millis();
      forceUpdate = false;
      return;
    }

    if (WiFi.status() != WL_CONNECTED) {
      setStatus("Waiting for WiFi connection", false);
      lastAttempt = millis();
      forceUpdate = false;
      return;
    }

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    String url;

    if (dynDnsConfig.provider == "duckdns") {
      url = "https://www.duckdns.org/update?domains=" + dynDnsConfig.hostname +
            "&token=" + dynDnsConfig.token + "&ip=";
    } else {
      url = "https://members.dyndns.org/nic/update?hostname=" + dynDnsConfig.hostname;
    }

    if (!http.begin(client, url)) {
      setStatus("Failed to contact DynDNS endpoint", false);
      lastAttempt = millis();
      forceUpdate = false;
      return;
    }

    if (dynDnsConfig.provider == "duckdns") {
      // DuckDNS uses token in query params; no auth header needed
    } else {
      http.setAuthorization(dynDnsConfig.username.c_str(), dynDnsConfig.password.c_str());
    }
    http.setUserAgent("ESP32-SignalK/1.0");

    int code = http.GET();

    if (code > 0) {
      String payload = http.getString();
      payload.trim();
      bool success;
      if (dynDnsConfig.provider == "duckdns") {
        success = payload == "OK";
      } else {
        success = code == 200 && (payload.startsWith("good") || payload.startsWith("nochg"));
      }
      setStatus("HTTP " + String(code) + ": " + payload, success);
    } else {
      setStatus("HTTP error: " + String(code), false);
    }

    http.end();
    forceUpdate = false;
    lastAttempt = millis();
  }
}  // namespace

void initDynDnsService() {
  forceUpdate = dynDnsConfig.enabled;
  lastAttempt = 0;
  if (!dynDnsConfig.lastResult.length()) {
    dynDnsConfig.lastResult = "DynDNS not updated yet";
  }
}

void processDynDnsService() {
  if (!dynDnsConfig.enabled && !forceUpdate) {
    return;
  }

  uint32_t now = millis();
  if (forceUpdate || (now - lastAttempt >= kDynDnsIntervalMs)) {
    performUpdate();
  }
}

void requestDynDnsUpdate() {
  forceUpdate = true;
}
