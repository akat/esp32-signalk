# API Module - ESP32 SignalK Server

This directory contains the REST API, WebSocket, and HTTP route handling for the ESP32 SignalK server.

## Module Organization

### security.h / security.cpp
**Purpose:** Authentication and token validation

**Key Functions:**
- `extractBearerToken()` - Extract Bearer tokens from Authorization headers
- `isTokenValid()` - Check if a token is approved

**Use Cases:**
- Validating client tokens before processing requests
- Securing sensitive endpoints

---

### handlers.h / handlers.cpp
**Purpose:** HTTP request handlers for all API endpoints and web UIs

**Handler Categories:**

1. **SignalK API Handlers**
   - `/signalk` - Discovery endpoint
   - `/signalk/v1/api` - API root
   - `/signalk/v1/api/vessels/self` - Vessel data
   - `/signalk/v1/api/vessels/self/*` - Path-based data access
   - `/signalk/v1/auth/validate` - Token validation
   - `/signalk/v1/access/requests` - Access request management

2. **Web UI Handlers**
   - `/` - Main dashboard with WebSocket viewer
   - `/config` - TCP configuration interface
   - `/admin` - Token management interface

3. **Admin API Handlers**
   - `/api/admin/tokens` - List tokens and requests
   - `/api/admin/approve/{requestId}` - Approve access
   - `/api/admin/deny/{requestId}` - Deny access
   - `/api/admin/revoke/{token}` - Revoke token

4. **Configuration Handlers**
   - `/api/tcp/config` - Get/set TCP connection settings

5. **Push Notification Handlers**
   - `/plugins/signalk-node-red/redApi/register-expo-token` - Register push tokens

**Embedded HTML UIs:**
- `HTML_UI` - Main dashboard
- `HTML_CONFIG` - Configuration interface
- `HTML_ADMIN` - Admin interface

---

### routes.h / routes.cpp
**Purpose:** Route registration and HTTP server setup

**Key Function:**
- `setupRoutes(AsyncWebServer& server)` - Registers all routes and configures CORS

**Responsibilities:**
- Register all HTTP GET/POST/PUT handlers
- Configure WebSocket endpoint
- Set up CORS headers
- Configure request body streaming
- Order routes correctly (specific before generic)

---

## Dependencies

### External Global Variables (from main.cpp)
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

### External Functions (from services/ or utils/)
```cpp
extern String generateUUID();
extern void setPathValue(...);
extern void setPathValueJson(...);
extern void saveApprovedTokens();
extern bool addExpoToken(const String& token);
extern void onWebSocketEvent(...);
```

### Libraries
- `<Arduino.h>` - Arduino framework
- `<ESPAsyncWebServer.h>` - Async web server
- `<ArduinoJson.h>` - JSON handling
- `<WiFi.h>` - WiFi connectivity

---

## Integration

### In main.cpp

1. **Add Includes:**
```cpp
#include "api/handlers.h"
#include "api/routes.h"
#include "api/security.h"
```

2. **In setup():**
```cpp
void setup() {
  // ... initialization code ...

  // Set up WebSocket event handler
  ws.onEvent(onWebSocketEvent);

  // Register all API routes
  setupRoutes(server);

  // Start server
  server.begin();
}
```

3. **For Token Validation:**
```cpp
#include "api/security.h"

// In any handler function:
String token = extractBearerToken(request);
if (!isTokenValid(token)) {
  request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
  return;
}
```

---

## API Endpoints

### SignalK Discovery
```
GET /signalk
Response: JSON with server info and API endpoints
```

### SignalK REST API
```
GET /signalk/v1/api/vessels/self
Response: Full vessel data with all paths

GET /signalk/v1/api/vessels/self/{path}
Response: Specific path value

PUT /signalk/v1/api/vessels/self/{path}
Body: {"value": X} or {"value": X, "source": "...", "description": "..."}
Response: Status confirmation
```

### Authentication
```
POST /signalk/v1/access/requests
Body: {"clientId": "...", "description": "...", "permissions": "readwrite"}
Response: Access request status with href for polling

GET /signalk/v1/access/requests/{requestId}
Response: Current status of request (PENDING or COMPLETED with token)

GET /signalk/v1/auth/validate
Response: Token validation result
```

### Admin Management
```
GET /api/admin/tokens
Response: List of pending requests and approved tokens

POST /api/admin/approve/{requestId}
Response: Success and generated token

POST /api/admin/deny/{requestId}
Response: Success confirmation

POST /api/admin/revoke/{token}
Response: Success confirmation
```

### Configuration
```
GET /api/tcp/config
Response: Current TCP configuration

POST /api/tcp/config
Body: {"host": "...", "port": 10110, "enabled": true}
Response: Success confirmation
```

### WebSocket
```
WS /signalk/v1/stream
Subscribe: {"context": "*", "subscribe": [{"path": "*", "period": 1000}]}
Receive: Delta format updates with values
```

---

## Special Features

### Navigation Anchor Configuration
The `/signalk/v1/api/vessels/self/navigation/anchor/akat` endpoint has special handling:
- Tracks depth alarm, wind alarm, and anchor position changes
- Only updates geofence state based on context (prevents conflicts)
- Auto-corrects and persists state to prevent sync issues
- Used by mobile apps for managing alarms and geofence

### SensESP Compatibility
- Auto-approves access requests from known clients
- Generates tokens after reboot if client has old requestId
- Permissive authentication for WebSocket (no auth required)
- Returns HTTP 426 for successful token validation

### CORS Support
All endpoints support cross-origin requests with these headers:
- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type, Authorization`

---

## Performance Considerations

1. **JSON Parsing:** Limited to 2048 bytes for PUT requests (adjust `DynamicJsonDocument` size if needed)
2. **Path Storage:** Using std::map for O(log n) lookup
3. **WebSocket:** Delta format compression for bandwidth efficiency
4. **HTML UIs:** Served as raw strings (consider minification for production)

---

## Debugging

### Enable Serial Debug Output
All handlers print extensive debug information to Serial:
```cpp
Serial.println("=== Handler Name ===");
Serial.printf("Parameter: %s\n", value);
// ...
Serial.println("===========================\n");
```

Monitor with Arduino IDE or platformio CLI:
```bash
platformio device monitor -p /dev/ttyUSB0 -b 115200
```

### Common Issues

**WebSocket not connecting:**
- Check firewall port 3000
- Verify ws.onEvent() called before setupRoutes()
- Check browser console for errors

**404 on endpoints:**
- Verify endpoint spelling
- Check route registration in routes.cpp
- Enable ASYNCWEBSERVER_REGEX for regex pattern matching

**Authentication failures:**
- Ensure token is in approvedTokens map
- Check Bearer token format: "Bearer <token>"
- Verify saveApprovedTokens() persists to flash

---

## Future Improvements

1. **Modularization:** Split handlers.cpp into logical modules
2. **HTML Files:** Extract embedded HTML to separate files
3. **Configuration:** Move hardcoded values to config.h
4. **Error Handling:** Standardize error response format
5. **Logging:** Use dedicated logging module
6. **Testing:** Add unit tests for handlers
7. **Documentation:** Auto-generate API docs from code

---

## File Statistics

| File | Size | Lines | Purpose |
|------|------|-------|---------|
| security.h | 656 B | ~20 | Declarations |
| security.cpp | 605 B | ~20 | Implementation |
| handlers.h | 2.7 KB | ~70 | Declarations |
| handlers.cpp | 45 KB | 1300+ | All handlers + HTML |
| routes.h | 347 B | ~10 | Declaration |
| routes.cpp | 7.1 KB | 220 | Route setup |

**Total:** ~56 KB, ~1650 lines of code

---

## License

Same as parent project.

---

## References

- [SignalK Protocol](https://signalk.org/)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ArduinoJson](https://arduinojson.org/)

