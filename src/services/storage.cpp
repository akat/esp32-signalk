#include "storage.h"
#include "../config.h"
#include "../hardware/seatalk1.h"

extern DynDnsConfig dynDnsConfig;

// Global hardware and AP configuration
HardwareConfig hardwareConfig;
APConfig apConfig;

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

// Hardware configuration functions
void loadHardwareConfig() {
  prefs.begin("hardware", true);

  // Load GPS settings (defaults from config.h)
  hardwareConfig.gps_rx = prefs.getInt("gps_rx", GPS_RX);
  hardwareConfig.gps_tx = prefs.getInt("gps_tx", GPS_TX);
  hardwareConfig.gps_baud = prefs.getInt("gps_baud", GPS_BAUD);

  // Load RS485 settings
  hardwareConfig.rs485_rx = prefs.getInt("rs485_rx", NMEA_RX);
  hardwareConfig.rs485_tx = prefs.getInt("rs485_tx", NMEA_TX);
  hardwareConfig.rs485_de = prefs.getInt("rs485_de", NMEA_DE);
  hardwareConfig.rs485_de_enable = prefs.getInt("rs485_de_en", NMEA_DE_ENABLE);
  hardwareConfig.rs485_baud = prefs.getInt("rs485_baud", NMEA_BAUD);

  // Load Seatalk1 settings
#ifdef USE_SEATALK1
  hardwareConfig.seatalk1_rx = prefs.getInt("st1_rx", SEATALK1_RX);
  hardwareConfig.seatalk1_baud = prefs.getInt("st1_baud", SEATALK_BAUD);
#else
  hardwareConfig.seatalk1_rx = prefs.getInt("st1_rx", 32);
  hardwareConfig.seatalk1_baud = prefs.getInt("st1_baud", 4800);
#endif

  // Load Single-Ended NMEA settings
#ifdef USE_SINGLEENDED_NMEA
  hardwareConfig.singleended_rx = prefs.getInt("se_rx", SINGLEENDED_NMEA_RX);
  hardwareConfig.singleended_baud = prefs.getInt("se_baud", SINGLEENDED_NMEA_BAUD);
#else
  hardwareConfig.singleended_rx = prefs.getInt("se_rx", 33);
  hardwareConfig.singleended_baud = prefs.getInt("se_baud", 4800);
#endif

  // Load CAN settings
  hardwareConfig.can_rx = prefs.getInt("can_rx", CAN_RX_PIN);
  hardwareConfig.can_tx = prefs.getInt("can_tx", CAN_TX_PIN);

  prefs.end();

  Serial.println("\n=== Hardware Configuration Loaded ===");
  Serial.printf("GPS: RX=%d, TX=%d, Baud=%d\n", hardwareConfig.gps_rx, hardwareConfig.gps_tx, hardwareConfig.gps_baud);
  Serial.printf("RS485: RX=%d, TX=%d, DE=%d, DE_EN=%d, Baud=%d\n",
    hardwareConfig.rs485_rx, hardwareConfig.rs485_tx, hardwareConfig.rs485_de,
    hardwareConfig.rs485_de_enable, hardwareConfig.rs485_baud);
  Serial.printf("Seatalk1: RX=%d, Baud=%d\n", hardwareConfig.seatalk1_rx, hardwareConfig.seatalk1_baud);
  Serial.printf("Single-Ended NMEA: RX=%d, Baud=%d\n", hardwareConfig.singleended_rx, hardwareConfig.singleended_baud);
  Serial.printf("CAN: RX=%d, TX=%d\n", hardwareConfig.can_rx, hardwareConfig.can_tx);
  Serial.println("=====================================\n");
}

void saveHardwareConfig(const HardwareConfig& config) {
  prefs.begin("hardware", false);

  // Save GPS settings
  prefs.putInt("gps_rx", config.gps_rx);
  prefs.putInt("gps_tx", config.gps_tx);
  prefs.putInt("gps_baud", config.gps_baud);

  // Save RS485 settings
  prefs.putInt("rs485_rx", config.rs485_rx);
  prefs.putInt("rs485_tx", config.rs485_tx);
  prefs.putInt("rs485_de", config.rs485_de);
  prefs.putInt("rs485_de_en", config.rs485_de_enable);
  prefs.putInt("rs485_baud", config.rs485_baud);

  // Save Seatalk1 settings
  prefs.putInt("st1_rx", config.seatalk1_rx);
  prefs.putInt("st1_baud", config.seatalk1_baud);

  // Save Single-Ended NMEA settings
  prefs.putInt("se_rx", config.singleended_rx);
  prefs.putInt("se_baud", config.singleended_baud);

  // Save CAN settings
  prefs.putInt("can_rx", config.can_rx);
  prefs.putInt("can_tx", config.can_tx);

  prefs.end();

  hardwareConfig = config;

  Serial.println("\n=== Hardware Configuration Saved ===");
  Serial.printf("GPS: RX=%d, TX=%d, Baud=%d\n", config.gps_rx, config.gps_tx, config.gps_baud);
  Serial.printf("RS485: RX=%d, TX=%d, DE=%d, DE_EN=%d, Baud=%d\n",
    config.rs485_rx, config.rs485_tx, config.rs485_de,
    config.rs485_de_enable, config.rs485_baud);
  Serial.printf("Seatalk1: RX=%d, Baud=%d\n", config.seatalk1_rx, config.seatalk1_baud);
  Serial.printf("Single-Ended NMEA: RX=%d, Baud=%d\n", config.singleended_rx, config.singleended_baud);
  Serial.printf("CAN: RX=%d, TX=%d\n", config.can_rx, config.can_tx);
  Serial.println("====================================\n");
}

// AP configuration functions
void loadAPConfig() {
  prefs.begin("ap_config", true);

  apConfig.ssid = prefs.getString("ap_ssid", AP_SSID);
  apConfig.password = prefs.getString("ap_pass", AP_PASSWORD);

  prefs.end();

  Serial.println("\n=== AP Configuration Loaded ===");
  Serial.printf("SSID: %s\n", apConfig.ssid.c_str());
  Serial.printf("Password: %s\n", apConfig.password.c_str());
  Serial.println("===============================\n");
}

void saveAPConfig(const APConfig& config) {
  prefs.begin("ap_config", false);

  prefs.putString("ap_ssid", config.ssid);
  prefs.putString("ap_pass", config.password);

  prefs.end();

  apConfig = config;

  Serial.println("\n=== AP Configuration Saved ===");
  Serial.printf("SSID: %s\n", config.ssid.c_str());
  Serial.printf("Password: %s\n", config.password.c_str());
  Serial.println("==============================\n");
  Serial.println("⚠️  RESTART REQUIRED for AP changes to take effect!");
}
