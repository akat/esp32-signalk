#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// ====== SIGNALK API HANDLERS ======

// GET /signalk - Discovery endpoint
void handleSignalKRoot(AsyncWebServerRequest* req);

// GET /signalk/v1/api - API root
void handleAPIRoot(AsyncWebServerRequest* req);

// GET /signalk/v1/access/requests - List access requests
void handleGetAccessRequests(AsyncWebServerRequest* req);

// GET /signalk/v1/access/requests/{requestId} - Poll for specific request status
void handleGetAccessRequestById(AsyncWebServerRequest* req);

// POST /signalk/v1/access/requests - Handle access requests
void handleAccessRequest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

// GET /signalk/v1/auth/validate - Validate authentication token
void handleAuthValidate(AsyncWebServerRequest* req);

// GET /signalk/v1/api/vessels/self - Get vessel data
void handleVesselsSelf(AsyncWebServerRequest* req);

// GET /signalk/v1/api/vessels/self/* - Get specific path
void handleGetPath(AsyncWebServerRequest* req);

// PUT /signalk/v1/api/vessels/self/* - Update path
void handlePutPath(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

// ====== WEB UI HANDLERS ======

// GET / - Serve main UI
void handleRoot(AsyncWebServerRequest* req);

// GET /config - Serve TCP configuration UI
void handleConfig(AsyncWebServerRequest* req);

// GET /admin - Serve token management UI
void handleAdmin(AsyncWebServerRequest* req);

// ====== ADMIN API HANDLERS ======

// GET /api/admin/tokens - Get all tokens and pending requests
void handleGetAdminTokens(AsyncWebServerRequest* req);

// POST /api/admin/approve/{requestId} - Approve access request
void handleApproveToken(AsyncWebServerRequest* req);

// POST /api/admin/deny/{requestId} - Deny access request
void handleDenyToken(AsyncWebServerRequest* req);

// POST /api/admin/revoke/{token} - Revoke approved token
void handleRevokeToken(AsyncWebServerRequest* req);

// POST /api/admin/* - Router for admin API endpoints
void handleAdminApiPost(AsyncWebServerRequest* req);

// ====== CONFIGURATION HANDLERS ======

// GET /api/tcp/config - Get TCP configuration
void handleGetTcpConfig(AsyncWebServerRequest* req);

// POST /api/tcp/config - Set TCP configuration
void handleSetTcpConfig(AsyncWebServerRequest* req, uint8_t *data, size_t len, size_t index, size_t total);

// GET /api/dyndns/config - Get Dynamic DNS configuration
void handleGetDynDnsConfig(AsyncWebServerRequest* req);

// POST /api/dyndns/config - Update Dynamic DNS configuration
void handleSetDynDnsConfig(AsyncWebServerRequest* req, uint8_t *data, size_t len, size_t index, size_t total);

// POST /api/dyndns/update - Trigger immediate update
void handleTriggerDynDnsUpdate(AsyncWebServerRequest* req);

// ====== PUSH NOTIFICATION HANDLERS ======

// POST /plugins/signalk-node-red/redApi/register-expo-token - Register Expo push token
void handleRegisterExpoToken(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

#endif  // API_HANDLERS_H
