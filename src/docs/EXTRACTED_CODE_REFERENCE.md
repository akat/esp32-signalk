# Extracted Code Reference

## Source Information

All code in the API modules was extracted from:
- **File:** `c:\Users\akatsaris\projects\esp32-signalk\src\main_original.cpp`
- **Total lines in original:** 4098
- **Code extracted:** Lines 373-3054 (handlers and security functions)

---

## Security Functions Origin

### Location in main_original.cpp
- Lines 373-388: Security-related functions

### Functions Extracted:
```cpp
// Line 373
bool isTokenValid(const String& token) {
  return approvedTokens.find(token) != approvedTokens.end();
}

// Line 377
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
```

---

## Handler Functions Origin

All handlers located in main_original.cpp from lines 1591-3054

### SignalK API Handlers (Lines 1591-2281)
- `handleSignalKRoot()` - Line 1591
- `handleAPIRoot()` - Line 1624
- `handleAccessRequest()` - Line 1636
- `handleGetAccessRequestById()` - Line 1712
- `handleGetAccessRequests()` - Line 1797
- `handleAuthValidate()` - Line 1816
- `handleVesselsSelf()` - Line 1843
- `handleGetPath()` - Line 1999
- `handlePutPath()` - Line 2035

### Web UI Handlers (Lines 2285-2597)
- `handleRoot()` - Line 2437
- `handleConfig()` - Line 2595
- `handleAdmin()` - Line 2725

### HTML Constants (Lines 2287-2723)
- `HTML_UI` - Line 2287
- `HTML_CONFIG` - Line 2442
- `HTML_ADMIN` - Line 2600

### Admin API Handlers (Lines 2730-3054)
- `handleGetAdminTokens()` - Line 2730
- `handleAdminApiPost()` - Line 2761
- `handleApproveToken()` - Line 2858
- `handleDenyToken()` - Line 2906
- `handleRevokeToken()` - Line 2929
- `handleRegisterExpoToken()` - Line 2954
- `handleGetTcpConfig()` - Line 3002
- `handleSetTcpConfig()` - Line 3013

---

## Routes Origin

All route setup code extracted from setup() function starting at line 3464

### HTTP Stream Token Validation (Lines 3653-3691)
The `/signalk/v1/stream` GET handler for token validation

### WebSocket Setup (Lines 3693-3697)
WebSocket initialization code

### HTTP Routes (Lines 3706-3800+)
All the `server.on()` calls for route registration

### Request Body Handler (Lines 3785-3820+)
The `server.onRequestBody()` callback for POST/PUT handling

---

## Data Structures Used

All handler functions depend on these structures defined in main_original.cpp:

### Location: Lines 152-209

```cpp
// Line 153
struct PathValue {
  JsonVariant value;
  String strValue;
  double numValue;
  bool isNumeric = false;
  bool isJson = false;
  String jsonValue;
  String timestamp;
  String source;
  String units;
  String description;
  bool changed;
};

// Line 183
struct AccessRequestData {
  String requestId;
  String clientId;
  String description;
  String permissions;
  String token;
  String state;
  String permission;
  unsigned long timestamp;
};

// Line 196
struct ApprovedToken {
  String token;
  String clientId;
  String description;
  String permissions;
  unsigned long approvedAt;
};

// Line 405
struct GeofenceConfig {
  bool enabled = false;
  double anchorLat = NAN;
  double anchorLon = NAN;
  double radius = 100.0;
  uint32_t anchorTimestamp = 0;
  bool alarmActive = false;
  double lastDistance = NAN;
};

// Line 416
struct DepthAlarmConfig {
  bool enabled = false;
  double threshold = 2.0;
  bool alarmActive = false;
  double lastDepth = NAN;
  uint32_t lastSampleTime = 0;
};

// Line 425
struct WindAlarmConfig {
  bool enabled = false;
  double threshold = 10.0;
  bool alarmActive = false;
  double lastWindSpeed = NAN;
};
```

---

## Global Variables Used

### Location: Lines 112-209

```cpp
// Line 112
AsyncWebServer server(3000);

// Line 113
AsyncWebSocket ws("/signalk/v1/stream");

// Line 114
Preferences prefs;

// Line 121-122
String vesselUUID;
String serverName = "ESP32-SignalK";

// Line 134-140
String tcpServerHost = "";
int tcpServerPort = 10110;
bool tcpEnabled = false;
WiFiClient tcpClient;

// Line 170
std::map<String, PathValue> dataStore;

// Line 171
std::map<String, PathValue> lastSentValues;

// Line 179
std::map<uint32_t, ClientSubscription> clientSubscriptions;

// Line 193
std::map<String, AccessRequestData> accessRequests;

// Line 203
std::map<String, ApprovedToken> approvedTokens;

// Line 206
std::vector<String> expoTokens;
```

---

## Key Code Patterns

### 1. Bearer Token Extraction
```cpp
String token = extractBearerToken(req);
if (token.length() > 0) {
  if (!isTokenValid(token)) {
    req->send(401, "application/json", "{...}");
    return;
  }
}
```

### 2. JSON Response Building
```cpp
DynamicJsonDocument doc(512);
doc["field"] = value;
String output;
serializeJson(doc, output);
req->send(200, "application/json", output);
```

### 3. Path Data Retrieval
```cpp
String path = req->url().substring(String("/signalk/v1/api/vessels/self/").length());
path.replace("/", ".");

if (dataStore.count(path) == 0) {
  req->send(404, "application/json", "{\"error\":\"Path not found\"}");
  return;
}

PathValue& pv = dataStore[path];
```

### 4. Nested JSON Object Creation
```cpp
JsonObject nav = doc.createNestedObject("navigation");

// Navigate through nested structure
JsonObject current = nav;
// ... create intermediate objects ...
JsonObject value = current.createNestedObject(finalKey);
value["timestamp"] = kv.second.timestamp;
value["value"] = kv.second.numValue;
```

### 5. Request Body Handling
```cpp
void handler(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  // Only process when we have the complete body
  if (index + len != total) {
    return;  // Wait for more data
  }

  // Process the complete body
  DynamicJsonDocument doc(512);
  deserializeJson(doc, data, len);
}
```

---

## HTML Embedded in handlers.cpp

### HTML_UI
- Location: handlers.cpp (embedded)
- Original: main_original.cpp lines 2287-2435
- Size: ~150 lines
- Features:
  - WebSocket connection to /signalk/v1/stream
  - Real-time data table display
  - Status indicators
  - Auto-subscription to all paths

### HTML_CONFIG
- Location: handlers.cpp (embedded)
- Original: main_original.cpp lines 2442-2593
- Size: ~150 lines
- Features:
  - TCP server configuration form
  - Current configuration display
  - Troubleshooting guide
  - AJAX form submission

### HTML_ADMIN
- Location: handlers.cpp (embedded)
- Original: main_original.cpp lines 2600-2723
- Size: ~120 lines
- Features:
  - Pending requests table
  - Approved tokens table
  - Approve/Deny/Revoke buttons
  - Auto-refresh every 5 seconds

---

## Special Handling

### Anchor Configuration (navigation.anchor.akat)
The `handlePutPath()` function includes special logic for the anchor.akat path:
- Tracks changes to depth alarm, wind alarm, and anchor position
- Only updates geofence.enabled based on context
- Persists corrected configuration state
- Prevents sync conflicts between multiple configurations

Lines in handlers.cpp: ~2106-2257

### SensESP Compatibility
Several features ensure compatibility with SensESP:
- Auto-approval of access requests
- Auto-generation of tokens after reboot
- HTTP 426 response on /signalk/v1/stream validation
- Permissive token validation

---

## Dependencies on External Modules

The API handlers depend on functions that should be in other modules:

### From services/ (assumed location):
```cpp
// These need to be declared extern in handlers.cpp
extern String generateUUID();
extern void setPathValue(const String& path, double value, const String& source, const String& units, const String& description);
extern void setPathValueJson(const String& path, const String& jsonStr, const String& source, const String& units, const String& description);
extern void saveApprovedTokens();
extern bool addExpoToken(const String& token);
```

### From main or hardware/:
```cpp
extern void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
```

---

## Notes for Future Refactoring

1. **HTML Files:** Consider extracting embedded HTML into separate files:
   - `ui/dashboard.html`
   - `ui/config.html`
   - `ui/admin.html`

2. **Handler Organization:** Consider splitting handlers.cpp into:
   - `api_handlers.cpp` - SignalK API endpoints
   - `ui_handlers.cpp` - Web UI handlers
   - `admin_handlers.cpp` - Admin API handlers

3. **HTML UI:** Consider using a templating engine or React for more complex UIs

4. **Configuration:** Move hardcoded values to config.h:
   - Port 3000
   - API version 1.7.0
   - Server version 1.0.0

5. **Error Handling:** Add consistent error response format across all handlers

6. **Logging:** Consider using a logging module instead of Serial.println() everywhere

