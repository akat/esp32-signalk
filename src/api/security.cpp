#include "security.h"

/**
 * Extract Bearer token from Authorization header
 * Format: Authorization: Bearer <token>
 */
String extractBearerToken(AsyncWebServerRequest* request) {
  if (!request->hasHeader("Authorization")) {
    return "";
  }

  String authHeader = request->getHeader("Authorization")->value();
  if (authHeader.startsWith("Bearer ")) {
    return authHeader.substring(7); // Remove "Bearer " prefix
  }

  return "";
}

/**
 * Check if a token is in the approved tokens list
 */
bool isTokenValid(const String& token) {
  return approvedTokens.find(token) != approvedTokens.end();
}
