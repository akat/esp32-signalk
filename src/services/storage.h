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

#endif // SERVICES_STORAGE_H
