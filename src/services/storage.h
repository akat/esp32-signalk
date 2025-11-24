#ifndef SERVICES_STORAGE_H
#define SERVICES_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include <map>
#include <vector>
#include "../types.h"

// Forward declarations for external globals
extern Preferences prefs;

// Note: ApprovedToken struct is defined in types.h

// External globals for tokens
extern std::map<String, ApprovedToken> approvedTokens;
extern std::vector<String> expoTokens;

// TCP configuration externals
extern String tcpServerHost;
extern int tcpServerPort;
extern bool tcpEnabled;
extern DynDnsConfig dynDnsConfig;

// Token management functions
void saveApprovedTokens();
void loadApprovedTokens();
bool isTokenValid(const String& token);

// Expo token management
void saveExpoTokens();
void loadExpoTokens();
bool addExpoToken(const String& token);

// TCP configuration
void loadTcpConfig();
void saveTcpConfig(String host, int port, bool enabled);

// Dynamic DNS configuration
void loadDynDnsConfig();
void saveDynDnsConfig(const DynDnsConfig& config);

// Hardware configuration (GPIO pins and baud rates)
struct HardwareConfig {
  // GPS
  int gps_rx;
  int gps_tx;
  int gps_baud;

  // RS485 (NMEA)
  int rs485_rx;
  int rs485_tx;
  int rs485_de;
  int rs485_de_enable;
  int rs485_baud;

  // Seatalk1
  int seatalk1_rx;
  int seatalk1_baud;

  // Single-Ended NMEA 0183 (Direct connection)
  int singleended_rx;
  int singleended_baud;

  // CAN
  int can_rx;
  int can_tx;
};

extern HardwareConfig hardwareConfig;

void loadHardwareConfig();
void saveHardwareConfig(const HardwareConfig& config);

// Access Point configuration
struct APConfig {
  String ssid;
  String password;
};

extern APConfig apConfig;

void loadAPConfig();
void saveAPConfig(const APConfig& config);

#endif // SERVICES_STORAGE_H
