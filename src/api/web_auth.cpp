#include "web_auth.h"
#include "../services/storage.h"
#include <Preferences.h>
#include <mbedtls/md.h>

// Session storage
std::map<String, WebSession> webSessions;

// Stored credentials
static String storedUsername = "admin";
static String storedPasswordHash = "";  // SHA256 hash of password

// Default password: "12345"
static const char* DEFAULT_PASSWORD_HASH = "5994471abb01112afcc18159f6cc74b4f511b99806da59b3caf5a9c173cacfc5";

/**
 * Compute SHA256 hash of a string
 */
String sha256(const String& input) {
  byte hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)input.c_str(), input.length());
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);

  // Convert to hex string
  String hashString = "";
  for (int i = 0; i < 32; i++) {
    char hex[3];
    sprintf(hex, "%02x", hash[i]);
    hashString += hex;
  }

  return hashString;
}

/**
 * Initialize web authentication
 */
void initWebAuth() {
  Preferences prefs;
  prefs.begin("web_auth", false);

  // Load stored password hash
  storedPasswordHash = prefs.getString("password_hash", "");

  // If no password set, use default
  if (storedPasswordHash.length() == 0) {
    storedPasswordHash = DEFAULT_PASSWORD_HASH;
    prefs.putString("password_hash", storedPasswordHash);
    Serial.println("Web Auth: Using default password (12345)");
    Serial.println("⚠️  SECURITY WARNING: Change the default password!");
  } else {
    Serial.println("Web Auth: Password loaded from flash");
  }

  // Load username (always "admin" for now, but could be extended)
  storedUsername = prefs.getString("username", "admin");

  prefs.end();

  Serial.println("Web Authentication initialized");
  Serial.println("Username: " + storedUsername);
}

/**
 * Validate credentials
 */
bool validateWebCredentials(const String& username, const String& password) {
  // Check username
  if (username != storedUsername) {
    Serial.println("Web Auth: Invalid username: " + username);
    return false;
  }

  // Hash the provided password and compare
  String passwordHash = sha256(password);

  if (passwordHash == storedPasswordHash) {
    Serial.println("Web Auth: Login successful for user: " + username);
    return true;
  } else {
    Serial.println("Web Auth: Invalid password for user: " + username);
    return false;
  }
}

/**
 * Generate random session ID
 */
String generateSessionId() {
  String sessionId = "";
  for (int i = 0; i < 32; i++) {
    sessionId += String(random(0, 16), HEX);
  }
  return sessionId;
}

/**
 * Create new session
 */
String createWebSession(const String& username) {
  String sessionId = generateSessionId();
  unsigned long now = millis();

  WebSession session;
  session.sessionId = sessionId;
  session.username = username;
  session.createdAt = now;
  session.lastAccess = now;
  session.isValid = true;

  webSessions[sessionId] = session;

  Serial.println("Web Auth: Session created for " + username);
  Serial.println("Session ID: " + sessionId);

  return sessionId;
}

/**
 * Validate session
 */
bool validateWebSession(const String& sessionId) {
  if (sessionId.length() == 0) {
    return false;
  }

  auto it = webSessions.find(sessionId);
  if (it == webSessions.end()) {
    return false;
  }

  WebSession& session = it->second;

  // Check if session is valid
  if (!session.isValid) {
    return false;
  }

  // Check timeout
  unsigned long now = millis();
  if (now - session.lastAccess > WEB_SESSION_TIMEOUT_MS) {
    Serial.println("Web Auth: Session expired: " + sessionId);
    session.isValid = false;
    return false;
  }

  // Update last access time
  session.lastAccess = now;

  return true;
}

/**
 * Get session
 */
WebSession* getWebSession(const String& sessionId) {
  auto it = webSessions.find(sessionId);
  if (it == webSessions.end()) {
    return nullptr;
  }

  return &(it->second);
}

/**
 * Destroy session
 */
void destroyWebSession(const String& sessionId) {
  auto it = webSessions.find(sessionId);
  if (it != webSessions.end()) {
    Serial.println("Web Auth: Session destroyed: " + sessionId);
    webSessions.erase(it);
  }
}

/**
 * Clean up expired sessions
 */
void cleanupWebSessions() {
  unsigned long now = millis();
  int cleaned = 0;

  for (auto it = webSessions.begin(); it != webSessions.end();) {
    WebSession& session = it->second;

    // Remove if expired or invalid
    if (!session.isValid || (now - session.lastAccess > WEB_SESSION_TIMEOUT_MS)) {
      it = webSessions.erase(it);
      cleaned++;
    } else {
      ++it;
    }
  }

  if (cleaned > 0) {
    Serial.println("Web Auth: Cleaned up " + String(cleaned) + " expired sessions");
  }
}

/**
 * Change password
 */
bool changeWebPassword(const String& oldPassword, const String& newPassword) {
  // Validate old password
  if (!validateWebCredentials(storedUsername, oldPassword)) {
    Serial.println("Web Auth: Password change failed - invalid old password");
    return false;
  }

  // Validate new password
  if (newPassword.length() < 4) {
    Serial.println("Web Auth: Password change failed - new password too short");
    return false;
  }

  // Hash new password
  String newPasswordHash = sha256(newPassword);

  // Save to flash
  Preferences prefs;
  prefs.begin("web_auth", false);
  prefs.putString("password_hash", newPasswordHash);
  prefs.end();

  // Update in memory
  storedPasswordHash = newPasswordHash;

  Serial.println("Web Auth: Password changed successfully");

  // Invalidate all existing sessions (force re-login)
  for (auto& pair : webSessions) {
    pair.second.isValid = false;
  }

  return true;
}

/**
 * Get username
 */
String getWebUsername() {
  return storedUsername;
}

/**
 * Extract session cookie from request
 */
String extractSessionCookie(AsyncWebServerRequest* request) {
  if (!request->hasHeader("Cookie")) {
    return "";
  }

  String cookies = request->header("Cookie");

  // Parse cookies for session_id
  int start = cookies.indexOf("session_id=");
  if (start == -1) {
    return "";
  }

  start += 11; // Length of "session_id="
  int end = cookies.indexOf(";", start);

  if (end == -1) {
    return cookies.substring(start);
  } else {
    return cookies.substring(start, end);
  }
}

/**
 * Require web authentication
 */
bool requireWebAuth(AsyncWebServerRequest* request) {
  String sessionId = extractSessionCookie(request);

  if (validateWebSession(sessionId)) {
    return true;
  }

  // Not authenticated - redirect to login
  request->redirect("/login.html");
  return false;
}
