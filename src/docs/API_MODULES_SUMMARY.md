# ESP32-SignalK API Modules - Restructuring Summary

## Overview
This document summarizes the new API module structure extracted from `main_original.cpp` for the ESP32-SignalK project restructuring.

## Files Created

### 1. **api/security.h** and **api/security.cpp**
**Location:** `c:\Users\akatsaris\projects\esp32-signalk\src\api\`

**Purpose:** Authentication and token validation functions

**Functions:**
- `String extractBearerToken(AsyncWebServerRequest* request)` - Extracts Bearer token from Authorization header
- `bool isTokenValid(const String& token)` - Validates if a token is in the approved tokens list

**External Dependencies:**
- `approvedTokens` - global map of approved tokens

---

### 2. **api/handlers.h** and **api/handlers.cpp**
**Location:** `c:\Users\akatsaris\projects\esp32-signalk\src\api\`

**Purpose:** All REST API request handlers and HTML UI serving

**Handler Functions:**

#### SignalK API Handlers
- `handleSignalKRoot()` - GET /signalk - Discovery endpoint
- `handleAPIRoot()` - GET /signalk/v1/api - API root
- `handleGetAccessRequests()` - GET /signalk/v1/access/requests - List requests
- `handleGetAccessRequestById()` - GET /signalk/v1/access/requests/{requestId} - Poll status
- `handleAccessRequest()` - POST /signalk/v1/access/requests - Handle access requests
- `handleAuthValidate()` - GET /signalk/v1/auth/validate - Validate tokens
- `handleVesselsSelf()` - GET /signalk/v1/api/vessels/self - Get vessel data
- `handleGetPath()` - GET /signalk/v1/api/vessels/self/* - Get specific path
- `handlePutPath()` - PUT /signalk/v1/api/vessels/self/* - Update path (with anchor.akat special handling)

#### Web UI Handlers
- `handleRoot()` - GET / - Main dashboard UI
- `handleConfig()` - GET /config - TCP configuration UI
- `handleAdmin()` - GET /admin - Token management UI

#### Admin API Handlers
- `handleGetAdminTokens()` - GET /api/admin/tokens - Get tokens and pending requests
- `handleApproveToken()` - POST /api/admin/approve/{requestId}
- `handleDenyToken()` - POST /api/admin/deny/{requestId}
- `handleRevokeToken()` - POST /api/admin/revoke/{token}
- `handleAdminApiPost()` - POST router for admin endpoints

#### Configuration Handlers
- `handleGetTcpConfig()` - GET /api/tcp/config - Get TCP settings
- `handleSetTcpConfig()` - POST /api/tcp/config - Save TCP settings

#### Push Notification Handlers
- `handleRegisterExpoToken()` - POST /plugins/signalk-node-red/redApi/register-expo-token

**Embedded HTML UIs:**
- `HTML_UI` - Main dashboard interface (WebSocket data viewer)
- `HTML_CONFIG` - TCP configuration interface
- `HTML_ADMIN` - Token management interface

**External Dependencies:**
- Global variables: `server`, `vesselUUID`, `serverName`, `dataStore`, `accessRequests`, `approvedTokens`, `expoTokens`, `tcpServerHost`, `tcpServerPort`, `tcpEnabled`, `tcpClient`, `lastSentValues`, `notifications`, `geofence`, `depthAlarm`, `windAlarm`, `prefs`
- Functions: `generateUUID()`, `setPathValue()`, `setPathValueJson()`, `saveApprovedTokens()`, `addExpoToken()`

---

### 3. **api/routes.h** and **api/routes.cpp**
**Location:** `c:\Users\akatsaris\projects\esp32-signalk\src\api\`

**Purpose:** Route registration and setup

**Key Function:**
- `void setupRoutes(AsyncWebServer& server)` - Registers all HTTP routes and WebSocket handlers

**Routes Registered:**
- WebSocket token validation endpoint (/signalk/v1/stream)
- WebSocket setup
- All web UI routes
- All SignalK API endpoints
- Admin API routes
- TCP configuration API
- Push notification API
- CORS headers configuration
- Request body handlers for POST/PUT operations

**Special Features:**
- Supports both regex and non-regex route matching
- Proper route ordering (specific routes before general ones)
- CORS headers configuration
- Request body handling for streaming data
- Fallback handlers for complex path matching

---

## Integration Notes

### How to Use These Modules

1. **Include in main.cpp:**
   ```cpp
   #include "api/handlers.h"
   #include "api/routes.h"
   #include "api/security.h"
   ```

2. **Call from setup():**
   ```cpp
   void setup() {
     // ... existing setup code ...
     setupRoutes(server);
   }
   ```

3. **Token validation in your code:**
   ```cpp
   String token = extractBearerToken(request);
   if (!isTokenValid(token)) {
     // Handle unauthorized
   }
   ```

### Global Variables Required

The following global variables must be declared in your main code before calling the handlers:

```cpp
extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern Preferences prefs;
extern String vesselUUID;
extern String serverName;
extern std::map<String, PathValue> dataStore;
extern std::map<String, AccessRequestData> accessRequests;
extern std::map<String, ApprovedToken> approvedTokens;
extern std::vector<String> expoTokens;
extern String tcpServerHost;
extern int tcpServerPort;
extern bool tcpEnabled;
extern WiFiClient tcpClient;
extern struct GeofenceConfig geofence;
extern struct DepthAlarmConfig depthAlarm;
extern struct WindAlarmConfig windAlarm;
```

### Utility Functions Required

These functions must be implemented elsewhere (likely in services/ module):

- `String generateUUID()` - Generate UUID strings
- `void setPathValue(const String& path, double value, const String& source, const String& units, const String& description)` - Store numeric values
- `void setPathValueJson(const String& path, const String& jsonStr, const String& source, const String& units, const String& description)` - Store JSON values
- `void saveApprovedTokens()` - Persist tokens to flash
- `bool addExpoToken(const String& token)` - Add Expo push token

---

## Key Features Preserved

1. **Authentication & Access Control**
   - Bearer token extraction and validation
   - Auto-approval system for SensESP compatibility
   - Admin token management UI

2. **SignalK Compliance**
   - Full REST API structure
   - WebSocket streaming with delta format
   - Proper response codes (202 for PENDING, 426 for Upgrade Required)

3. **Data Management**
   - Path-based data storage and retrieval
   - JSON object support (anchor.akat with special handling)
   - Metadata (units, description, source) preservation

4. **Web UI**
   - Real-time data monitoring dashboard
   - TCP configuration interface
   - Admin token management interface
   - CORS support for cross-origin requests

5. **Push Notifications**
   - Expo push token registration
   - Rate limiting and validation

---

## File Statistics

| File | Lines | Purpose |
|------|-------|---------|
| security.h | ~22 | Security function declarations |
| security.cpp | ~20 | Token extraction and validation |
| handlers.h | ~70 | Handler function declarations |
| handlers.cpp | ~1300+ | All handler implementations + embedded HTML |
| routes.h | ~12 | Routes setup function declaration |
| routes.cpp | ~220 | Route registration and WebSocket setup |

---

## Next Steps

1. Update your main.cpp to include these new modules
2. Remove the handler functions and route setup code from main_original.cpp
3. Ensure all extern declarations match the actual global variables in main.cpp
4. Test each endpoint to verify proper functionality
5. Consider extracting the HTML UIs into separate files if they become too large

---

## Architecture Benefits

- **Modularity:** All API logic is now in a dedicated module
- **Maintainability:** Handlers are organized by function
- **Reusability:** Security module can be used in other parts of the project
- **Clarity:** Route setup is explicit and easy to modify
- **Testability:** Individual handlers can be tested independently

