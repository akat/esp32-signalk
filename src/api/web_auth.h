#ifndef API_WEB_AUTH_H
#define API_WEB_AUTH_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <map>

/**
 * Web Authentication Module
 *
 * Provides session-based authentication for the web interface.
 * Separate from SignalK token authentication.
 *
 * Default credentials:
 * - Username: admin
 * - Password: 12345
 */

// Session management
struct WebSession {
  String sessionId;
  String username;
  unsigned long createdAt;
  unsigned long lastAccess;
  bool isValid;
};

// Global session storage
extern std::map<String, WebSession> webSessions;

/**
 * Initialize web authentication system
 * Loads stored password from flash or sets default
 */
void initWebAuth();

/**
 * Validate username and password
 *
 * @param username Username to validate
 * @param password Password to validate
 * @return true if credentials are valid
 */
bool validateWebCredentials(const String& username, const String& password);

/**
 * Create new session for authenticated user
 *
 * @param username Username to create session for
 * @return Session ID (cookie value)
 */
String createWebSession(const String& username);

/**
 * Validate session from cookie
 *
 * @param sessionId Session ID from cookie
 * @return true if session is valid
 */
bool validateWebSession(const String& sessionId);

/**
 * Get session from session ID
 *
 * @param sessionId Session ID
 * @return Pointer to WebSession or nullptr if not found
 */
WebSession* getWebSession(const String& sessionId);

/**
 * Destroy session (logout)
 *
 * @param sessionId Session ID to destroy
 */
void destroyWebSession(const String& sessionId);

/**
 * Clean up expired sessions
 * Should be called periodically
 */
void cleanupWebSessions();

/**
 * Change web password
 *
 * @param oldPassword Current password
 * @param newPassword New password
 * @return true if password was changed successfully
 */
bool changeWebPassword(const String& oldPassword, const String& newPassword);

/**
 * Get current username (always "admin")
 *
 * @return Current username
 */
String getWebUsername();

/**
 * Extract session ID from request cookie
 *
 * @param request AsyncWebServerRequest pointer
 * @return Session ID or empty string if not found
 */
String extractSessionCookie(AsyncWebServerRequest* request);

/**
 * Check if request has valid session
 * If not, redirects to login page
 *
 * @param request AsyncWebServerRequest pointer
 * @return true if authenticated, false otherwise
 */
bool requireWebAuth(AsyncWebServerRequest* request);

/**
 * Generate random session ID
 *
 * @return Random session ID
 */
String generateSessionId();

// Session timeout (30 minutes)
#define WEB_SESSION_TIMEOUT_MS 1800000

// Session cleanup interval (5 minutes)
#define WEB_SESSION_CLEANUP_MS 300000

#endif // API_WEB_AUTH_H
