# 6Pack App Integration with ESP32 SignalK

## How 6Pack Sends Anchor Configuration

The 6Pack app sends anchor and alarm configuration to SignalK using **HTTP PUT** requests.

### Request Details

**URL Pattern:**
```
PUT http://192.168.4.1:3000/signalk/v1/api/vessels/self/navigation/anchor/akat
```

**Headers:**
```
Content-Type: application/json
```

**Body (JSON):**
```json
{
  "anchor": {
    "enabled": false,
    "radius": 30,
    "lat": 59.88183333333333,
    "lon": 23.346116666666667,
    "ts": 1762839677604
  },
  "depth": {
    "alarm": false,
    "min_depth": 2.5
  },
  "wind": {
    "alarm": false,
    "max_speed": 50
  }
}
```

## How ESP32 Handles This

### Current Implementation

The ESP32 PUT handler ([main.cpp:525-597](src/main.cpp#L525-L597)) processes this request as follows:

1. **URL Parsing**: Extracts path `navigation.anchor.akat` from URL
2. **JSON Parsing**: Deserializes the complex JSON object
3. **Storage**: Stores the entire object as a **serialized JSON string**
4. **Response**: Returns success with `state: COMPLETED`

### Storage Format

The data is stored in the global `dataStore` map with:
- **Key**: `"navigation.anchor.akat"`
- **Value**: PathValue struct containing:
  - `strValue`: The full JSON object as a string
  - `isNumeric`: false
  - `timestamp`: ISO8601 timestamp
  - `source`: "app" (from request or default)

### Viewing the Data

**Via HTTP GET:**
```bash
curl http://192.168.4.1:3000/signalk/v1/api/vessels/self/navigation/anchor/akat
```

**Response:**
```json
{
  "value": "{\"anchor\":{\"enabled\":false,\"radius\":30,\"lat\":59.88183333333333,\"lon\":23.346116666666667,\"ts\":1762839677604},\"depth\":{\"alarm\":false,\"min_depth\":2.5},\"wind\":{\"alarm\":false,\"max_speed\":50}}",
  "timestamp": "2025-01-11T12:34:56.000Z",
  "$source": "app"
}
```

**Via Web Interface:**
Navigate to http://192.168.4.1:3000/ to see the data in the navigation table.

## Testing

### Using PowerShell
Run the included test script:
```powershell
.\test_anchor_put.ps1
```

### Using curl
```bash
curl -X PUT http://192.168.4.1:3000/signalk/v1/api/vessels/self/navigation/anchor/akat \
  -H "Content-Type: application/json" \
  -d '{
    "anchor": {
      "enabled": false,
      "radius": 30,
      "lat": 59.88183333333333,
      "lon": 23.346116666666667,
      "ts": 1762839677604
    },
    "depth": {
      "alarm": false,
      "min_depth": 2.5
    },
    "wind": {
      "alarm": false,
      "max_speed": 50
    }
  }'
```

### Using Postman or Insomnia
- **Method**: PUT
- **URL**: `http://192.168.4.1:3000/signalk/v1/api/vessels/self/navigation/anchor/akat`
- **Headers**: `Content-Type: application/json`
- **Body**: Raw JSON (see above)

## Debugging

The ESP32 will log detailed information to the serial console:

```
=== PUT PATH REQUEST ===
Path: navigation.anchor.akat
Data length: 234 bytes
Raw body: {"anchor":{"enabled":false...
Parsed JSON:
{
  "anchor": {
    "enabled": false,
    "radius": 30,
    "lat": 59.88183333333333,
    "lon": 23.346116666666667,
    "ts": 1762839677604
  },
  "depth": {
    "alarm": false,
    "min_depth": 2.5
  },
  "wind": {
    "alarm": false,
    "max_speed": 50
  }
}
Set object/array value: {"anchor":{"enabled":false...
Path updated successfully
```

## Troubleshooting

### 6Pack App Not Sending Data

**Check:**
1. ✅ Mobile device connected to `ESP32-SignalK` WiFi network
2. ✅ 6Pack configured with:
   - Host: `192.168.4.1`
   - Port: `3000`
   - Protocol: HTTP (not HTTPS)
3. ✅ No authentication configured (ESP32 has no auth)

### Data Not Appearing

**Monitor serial output:**
- Connect to ESP32 via USB
- Open serial monitor at 115200 baud
- Look for `=== PUT PATH REQUEST ===` when 6Pack sends data
- Check for any errors in JSON parsing

### Data Format Issues

The ESP32 stores complex objects as JSON strings. When retrieved:
- The `value` field will contain the entire JSON as a string
- You may need to parse it again in your client application
- The web interface displays it as-is

## Expected Behavior

When you configure anchor settings in 6Pack:

1. **6Pack sends PUT** → `navigation.anchor.akat` with full configuration
2. **ESP32 receives** → Logs request to serial console
3. **ESP32 stores** → Saves JSON object as string in dataStore
4. **ESP32 responds** → Returns `{"state":"COMPLETED","statusCode":200}`
5. **Data available** → Can GET via API or view in web interface
6. **WebSocket broadcast** → All connected clients receive delta update

The path `navigation.anchor.akat` will appear in the web UI table with the full JSON object as its value.
