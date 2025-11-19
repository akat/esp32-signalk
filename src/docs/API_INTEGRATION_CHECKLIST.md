# API Module Integration Checklist

## Step-by-Step Integration Guide

### Phase 1: Verify File Structure
- [ ] Confirm all 6 files exist in `src/api/`:
  - [ ] `security.h`
  - [ ] `security.cpp`
  - [ ] `handlers.h`
  - [ ] `handlers.cpp`
  - [ ] `routes.h`
  - [ ] `routes.cpp`

### Phase 2: Update main.cpp Includes
- [ ] Add to the includes section of main.cpp:
  ```cpp
  #include "api/handlers.h"
  #include "api/routes.h"
  #include "api/security.h"
  ```

### Phase 3: Verify Global Variable Declarations
In main.cpp, ensure these are declared (should already exist):
- [ ] `AsyncWebServer server(3000);`
- [ ] `AsyncWebSocket ws("/signalk/v1/stream");`
- [ ] `Preferences prefs;`
- [ ] `String vesselUUID;`
- [ ] `String serverName;`
- [ ] `std::map<String, PathValue> dataStore;`
- [ ] `std::map<String, AccessRequestData> accessRequests;`
- [ ] `std::map<String, ApprovedToken> approvedTokens;`
- [ ] `std::vector<String> expoTokens;`
- [ ] `String tcpServerHost;`
- [ ] `int tcpServerPort;`
- [ ] `bool tcpEnabled;`
- [ ] `WiFiClient tcpClient;`
- [ ] `struct GeofenceConfig geofence;`
- [ ] `struct DepthAlarmConfig depthAlarm;`
- [ ] `struct WindAlarmConfig windAlarm;`

### Phase 4: Verify Support Functions Exist
These functions must exist somewhere in the project (verify they're not in main_original.cpp):
- [ ] `String generateUUID()` - Check services/ or utils/ modules
- [ ] `void setPathValue(...)` - Check services/ module
- [ ] `void setPathValueJson(...)` - Check services/ module
- [ ] `void saveApprovedTokens()` - Check services/ module
- [ ] `bool addExpoToken(const String&)` - Check services/ module

### Phase 5: Remove Duplicates from main_original.cpp
Since handlers are now in the api module, remove these from your main.cpp if they exist:
- [ ] Remove `extractBearerToken()` function
- [ ] Remove `isTokenValid()` function
- [ ] Remove all `handleSignalKRoot()`, `handleAPIRoot()`, etc. functions
- [ ] Remove `HTML_UI`, `HTML_CONFIG`, `HTML_ADMIN` constants
- [ ] Remove route registration code from setup()

### Phase 6: Update setup() Function
In the setup() function, replace the old route registration code with:

```cpp
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 SignalK Server ===\n");

  // ... existing setup code ...

  // Load configuration, initialize hardware, etc.
  // ...

  // NEW: Register API routes (replaces old inline route registration)
  setupRoutes(server);

  // Start server
  server.begin();
  Serial.println("HTTP server started");
}
```

### Phase 7: Verify WebSocket Integration
- [ ] Ensure `ws.onEvent()` is called with your WebSocket event handler BEFORE `setupRoutes()`
- [ ] The setupRoutes function registers the ws handler, so this must be done first
- [ ] Check that onWebSocketEvent() is defined in your main.cpp

Example:
```cpp
void setup() {
  // ... initialization ...

  // Set up WebSocket event handler FIRST
  ws.onEvent(onWebSocketEvent);  // Your event handler

  // Then setup all routes (which adds the ws handler to server)
  setupRoutes(server);
}
```

### Phase 8: Compilation
- [ ] Run PlatformIO build
- [ ] Verify no compilation errors
- [ ] Check that all symbols are resolved
- [ ] Address any linker errors related to missing external functions

### Phase 9: Testing

#### Test Endpoints (in order):
1. **WebSocket Connection**
   - [ ] Connect to `ws://192.168.4.1:3000/signalk/v1/stream`
   - [ ] Send subscribe message
   - [ ] Receive delta updates

2. **REST API - Discovery**
   - [ ] GET `http://192.168.4.1:3000/signalk`
   - [ ] Verify JSON response with endpoints

3. **REST API - Data**
   - [ ] GET `http://192.168.4.1:3000/signalk/v1/api/vessels/self`
   - [ ] Verify vessel data structure

4. **Web UI**
   - [ ] GET `http://192.168.4.1:3000/`
   - [ ] Check if UI loads and WebSocket connects
   - [ ] GET `http://192.168.4.1:3000/config`
   - [ ] GET `http://192.168.4.1:3000/admin`

5. **PUT Endpoint**
   - [ ] PUT data to `/signalk/v1/api/vessels/self/navigation/position`
   - [ ] Verify it's stored and retrievable

6. **Access Requests**
   - [ ] POST to `/signalk/v1/access/requests` with client info
   - [ ] GET polling endpoint
   - [ ] Verify token generation

7. **Admin API**
   - [ ] GET `/api/admin/tokens`
   - [ ] POST to `/api/admin/approve/{requestId}`
   - [ ] Verify token management

### Phase 10: Cleanup
- [ ] Backup main_original.cpp if not already done
- [ ] Remove main_original.cpp from project (keep backup)
- [ ] Remove old inline route setup code from main.cpp
- [ ] Update CMakeLists.txt or platformio.ini if needed to exclude old files

---

## Troubleshooting

### Compilation Errors

**"undefined reference to `extractBearerToken`"**
- Ensure you have `#include "api/security.h"` in main.cpp
- Verify api/security.cpp is being compiled

**"error: 'approvedTokens' was not declared in this scope"**
- handlers.cpp uses `extern approvedTokens` - ensure it's declared in main.cpp
- Add to main.cpp if missing: `std::map<String, ApprovedToken> approvedTokens;`

**"undefined reference to `generateUUID`"**
- This function should be in services/ or utils/
- If missing, you'll need to implement it or find where it's defined
- Check if it's in a file that's not being compiled

### Runtime Errors

**WebSocket not connecting**
- Ensure `ws.onEvent(onWebSocketEvent)` is called BEFORE `setupRoutes(server)`
- Check serial output for any error messages
- Verify firewall isn't blocking port 3000

**Routes not responding**
- Check serial output when connecting - should see "Setting up HTTP routes"
- If not seeing this, setupRoutes() wasn't called
- Verify server.begin() is called after setupRoutes()

**404 on API endpoints**
- Make sure all routes were registered
- Check serial output for "HTTP routes setup complete"
- Verify endpoint spelling matches exactly

---

## Version Control

When committing these changes:

```bash
# Stage the new API module files
git add src/api/

# Stage the integration documentation
git add API_MODULES_SUMMARY.md
git add API_INTEGRATION_CHECKLIST.md

# Stage modifications to main.cpp
git add src/main.cpp

# Commit with a descriptive message
git commit -m "refactor: extract API handlers into modular structure

- Create api/security.h/cpp for authentication functions
- Create api/handlers.h/cpp with all REST API handlers
- Create api/routes.h/cpp for route registration
- Update main.cpp to use new API modules
- Remove duplicate handler code from main.cpp"
```

---

## Notes

- The handlers.cpp file is quite large (~1300 lines) - consider splitting into multiple files if it grows further
- HTML UIs are embedded as raw strings - consider extracting to separate HTML files for easier editing
- All handler functions maintain the same signatures as in main_original.cpp
- External dependencies are documented in forward declarations
- CORS headers are configured globally in setupRoutes()

