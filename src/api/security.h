#ifndef API_SECURITY_H
#define API_SECURITY_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include "../types.h"

// Forward declarations for global variables
extern std::map<String, ApprovedToken> approvedTokens;

/**
 * Extract Bearer token from Authorization header
 * @param request AsyncWebServerRequest pointer
 * @return Token string (without "Bearer " prefix), or empty string if not found
 */
String extractBearerToken(AsyncWebServerRequest* request);

/**
 * Validate if a token is approved
 * @param token Token string to validate
 * @return true if token is valid, false otherwise
 */
bool isTokenValid(const String& token);

#endif  // API_SECURITY_H
