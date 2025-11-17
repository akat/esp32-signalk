#include "storage.h"

extern DynDnsConfig dynDnsConfig;

// Token storage functions
void saveApprovedTokens() {
  prefs.begin("signalk", false);
  prefs.putInt("tokenCount", approvedTokens.size());

  int index = 0;
  for (auto& pair : approvedTokens) {
    String prefix = "tok" + String(index) + "_";
    prefs.putString((prefix + "token").c_str(), pair.second.token);
    prefs.putString((prefix + "clientId").c_str(), pair.second.clientId);
    prefs.putString((prefix + "desc").c_str(), pair.second.description);
    prefs.putString((prefix + "perms").c_str(), pair.second.permissions);
    prefs.putULong((prefix + "time").c_str(), pair.second.approvedAt);
    index++;
  }

  prefs.end();
  Serial.println("Approved tokens saved to flash");
}

void loadApprovedTokens() {
  prefs.begin("signalk", true);
  int tokenCount = prefs.getInt("tokenCount", 0);

  Serial.printf("Loading %d approved tokens from flash...\n", tokenCount);

  for (int i = 0; i < tokenCount; i++) {
    String prefix = "tok" + String(i) + "_";
    ApprovedToken token;
    token.token = prefs.getString((prefix + "token").c_str(), "");
    token.clientId = prefs.getString((prefix + "clientId").c_str(), "");
    token.description = prefs.getString((prefix + "desc").c_str(), "");
    token.permissions = prefs.getString((prefix + "perms").c_str(), "read");
    token.approvedAt = prefs.getULong((prefix + "time").c_str(), 0);

    if (token.token.length() > 0) {
      approvedTokens[token.token] = token;
      Serial.printf("  Loaded token for: %s\n", token.clientId.c_str());
    }
  }

  prefs.end();
}

// Note: isTokenValid() is defined in api/security.cpp

// Expo token management
void saveExpoTokens() {
  prefs.begin("signalk", false);
  prefs.putInt("expoCount", expoTokens.size());
  for (size_t i = 0; i < expoTokens.size(); i++) {
    String key = "expo" + String(i);
    prefs.putString(key.c_str(), expoTokens[i]);
  }
  prefs.end();
  Serial.printf("Saved %d Expo tokens\n", expoTokens.size());
}

void loadExpoTokens() {
  prefs.begin("signalk", true);
  int count = prefs.getInt("expoCount", 0);
  expoTokens.clear();
  for (int i = 0; i < count; i++) {
    String key = "expo" + String(i);
    String token = prefs.getString(key.c_str(), "");
    if (token.length() > 0) {
      expoTokens.push_back(token);
    }
  }
  prefs.end();
  Serial.printf("Loaded %d Expo tokens\n", expoTokens.size());
}

bool addExpoToken(const String& token) {
  // Check if token already exists
  for (const auto& existingToken : expoTokens) {
    if (existingToken == token) {
      return false;  // Already exists
    }
  }
  expoTokens.push_back(token);
  saveExpoTokens();
  return true;
}

// TCP configuration functions
void loadTcpConfig() {
  prefs.begin("signalk", false);
  tcpServerHost = prefs.getString("tcp_host", "");
  tcpServerPort = prefs.getInt("tcp_port", 10110);
  tcpEnabled = prefs.getBool("tcp_enabled", false);
  prefs.end();

  Serial.println("\n=== TCP Configuration ===");
  Serial.println("Host: " + tcpServerHost);
  Serial.println("Port: " + String(tcpServerPort));
  Serial.println("Enabled: " + String(tcpEnabled ? "Yes" : "No"));
  Serial.println("=========================\n");
}

void saveTcpConfig(String host, int port, bool enabled) {
  prefs.begin("signalk", false);
  prefs.putString("tcp_host", host);
  prefs.putInt("tcp_port", port);
  prefs.putBool("tcp_enabled", enabled);
  prefs.end();

  tcpServerHost = host;
  tcpServerPort = port;
  tcpEnabled = enabled;

  Serial.println("\n=== TCP Configuration Saved ===");
  Serial.println("Host: " + host);
  Serial.println("Port: " + String(port));
  Serial.println("Enabled: " + String(enabled ? "Yes" : "No"));
  Serial.println("===============================\n");
}

void loadDynDnsConfig() {
  prefs.begin("signalk", false);
  dynDnsConfig.provider = prefs.getString("dyndns_provider", "dyndns");
  if (dynDnsConfig.provider != "duckdns") {
    dynDnsConfig.provider = "dyndns";
  }
  dynDnsConfig.hostname = prefs.getString("dyndns_host", "");
  dynDnsConfig.username = prefs.getString("dyndns_user", "");
  dynDnsConfig.password = prefs.getString("dyndns_pass", "");
  dynDnsConfig.token = prefs.getString("dyndns_token", "");
  dynDnsConfig.enabled = prefs.getBool("dyndns_enabled", false);
  prefs.end();

  dynDnsConfig.lastResult = dynDnsConfig.enabled ? "DynDNS ready" : "DynDNS disabled";
  dynDnsConfig.lastUpdated = "";
  dynDnsConfig.lastSuccess = false;
  dynDnsConfig.lastUpdateMs = 0;
}

void saveDynDnsConfig(const DynDnsConfig& config) {
  prefs.begin("signalk", false);
  prefs.putString("dyndns_provider", config.provider);
  prefs.putString("dyndns_host", config.hostname);
  prefs.putString("dyndns_user", config.username);
  prefs.putString("dyndns_pass", config.password);
  prefs.putString("dyndns_token", config.token);
  prefs.putBool("dyndns_enabled", config.enabled);
  prefs.end();

  dynDnsConfig = config;
  dynDnsConfig.lastResult = config.enabled ? "Pending update" : "DynDNS disabled";
  dynDnsConfig.lastUpdated = "";
  dynDnsConfig.lastSuccess = false;
  dynDnsConfig.lastUpdateMs = 0;
}
