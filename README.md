# ESP32 SignalK Server

Πλήρης υλοποίηση SignalK server για ESP32 με WiFiManager, WebSocket deltas, REST API και NMEA0183 parsing.

## 🚀 Γρήγορη Εκκίνηση

### Απαιτήσεις Hardware

- ESP32 DevKit (οποιοδήποτε variant)
- GPS module με NMEA0183 output (4800 baud)
- Καλώδια σύνδεσης

### Καλωδίωση GPS

```
GPS Module    →    ESP32
---------          ------
TX        →    GPIO 16 (RX)
RX        →    GPIO 17 (TX)
VCC       →    3.3V
GND       →    GND
```

### Εγκατάσταση με PlatformIO

1. **Clone το project:**
```bash
git clone <your-repo>
cd esp32-signalk-server
```

2. **Άνοιξε στο VS Code με PlatformIO**

3. **Build & Upload:**
```bash
pio run --target upload
```

4. **Monitor:**
```bash
pio device monitor
```

### Εγκατάσταση με Arduino IDE

1. **Εγκατάστησε τις βιβλιοθήκες:**
   - Sketch → Include Library → Manage Libraries
   - Αναζήτησε και εγκατάστησε:
     - `WiFiManager` by tzapu (v2.0.16-rc.2+)
     - `ArduinoJson` by Benoit Blanchon (v6.x)
     - `ESPAsyncWebServer` και `AsyncTCP` (manual install από GitHub)

2. **Manual install για ESPAsyncWebServer:**
   - Download: https://github.com/me-no-dev/ESPAsyncWebServer/archive/master.zip
   - Download: https://github.com/me-no-dev/AsyncTCP/archive/master.zip
   - Extract στο `Arduino/libraries/`

3. **Άνοιξε το signalk_server.ino**

4. **Επίλεξε Board:** Tools → Board → ESP32 Dev Module

5. **Upload!**

## 📡 Πρώτη Χρήση

### 1. WiFi Setup

Κατά την πρώτη εκκίνηση:

1. Το ESP32 δημιουργεί Access Point: **`SignalK-Setup`**
2. Password: **`signalk123`**
3. Συνδέσου από το κινητό/laptop
4. Ανοίγει αυτόματα το configuration portal
5. Επίλεξε το WiFi δίκτυό σου και βάλε password
6. (Optional) Άλλαξε το "Server Name"
7. Πάτα Save

### 2. Εύρεση IP Address

Μετά τη σύνδεση, δες το Serial Monitor για το IP:
```
WiFi Connected!
IP Address: 192.168.1.100
SignalK API: http://192.168.1.100/signalk/v1/api/
WebSocket: ws://192.168.1.100/signalk/v1/stream
```

### 3. Access Points

- **Web UI:** http://192.168.1.100/
- **SignalK API:** http://192.168.1.100/signalk/v1/api/
- **WebSocket:** ws://192.168.1.100/signalk/v1/stream

## 🔐 Authentication

### Default Admin Token

Κατά την πρώτη εκκίνηση δημιουργείται αυτόματα ένα admin token:

```
=== DEFAULT ADMIN TOKEN ===
abc123def456ghi789...
===========================
```

**ΣΗΜΑΝΤΙΚΟ:** Κράτησε αυτό το token! Το χρειάζεσαι για admin operations.

### Χρήση Tokens

#### Με Authorization Header:
```bash
curl -H "Authorization: Bearer YOUR_TOKEN" \
  http://192.168.1.100/signalk/v1/api/vessels/self
```

#### Με Cookie:
```bash
curl -b "JAUTHENTICATION=YOUR_TOKEN" \
  http://192.168.1.100/signalk/v1/api/vessels/self
```

### Διαχείριση Tokens

**Λίστα όλων των tokens (admin only):**
```bash
curl -H "Authorization: Bearer ADMIN_TOKEN" \
  http://192.168.1.100/admin/tokens
```

**Δημιουργία νέου token:**
```bash
curl -X POST \
  -H "Authorization: Bearer ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"role":"readonly"}' \
  http://192.168.1.100/admin/tokens
```

Roles:
- `admin` - Full access (read/write)
- `readonly` - Read-only access

## 📊 SignalK API Usage

### Discovery
```bash
curl http://192.168.1.100/signalk
```

### Get All Vessel Data
```bash
curl http://192.168.1.100/signalk/v1/api/vessels/self
```

### Get Specific Path
```bash
curl http://192.168.1.100/signalk/v1/api/vessels/self/navigation/position
```

### Update Value (requires admin token)
```bash
curl -X PUT \
  -H "Authorization: Bearer ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"value": 45.5}' \
  http://192.168.1.100/signalk/v1/api/vessels/self/navigation/custom/myvalue
```

## 🔌 WebSocket Usage

### JavaScript Client
```javascript
const ws = new WebSocket('ws://192.168.1.100/signalk/v1/stream');

ws.onopen = () => {
  // Subscribe to all navigation paths
  ws.send(JSON.stringify({
    context: 'vessels.self',
    subscribe: [
      {
        path: 'navigation.*',
        period: 1000,
        format: 'delta',
        policy: 'instant'
      }
    ]
  }));
};

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Delta:', data);
};
```

### Python Client
```python
import websocket
import json

def on_message(ws, message):
    data = json.loads(message)
    print('Delta:', data)

def on_open(ws):
    subscribe = {
        "context": "vessels.self",
        "subscribe": [{
            "path": "*",
            "period": 1000
        }]
    }
    ws.send(json.dumps(subscribe))

ws = websocket.WebSocketApp(
    "ws://192.168.1.100/signalk/v1/stream",
    on_message=on_message,
    on_open=on_open
)
ws.run_forever()
```

## 🛠️ Προχωρημένη Διαμόρφωση

### Αλλαγή NMEA Pins

Στο `signalk_server.ino`:
```cpp
#define NMEA_RX 16  // Άλλαξε αυτό
#define NMEA_TX 17  // Άλλαξε αυτό
#define NMEA_BAUD 4800  // ή 38400 για AIS
```

### Πολλαπλές NMEA Πηγές

```cpp
// GPS στο Serial1
Serial1.begin(4800, SERIAL_8N1, 16, 17);

// AIS στο Serial2
Serial2.begin(38400, SERIAL_8N1, 18, 19);
```

### mDNS (signalk.local)

Πρόσθεσε στο `setup()`:
```cpp
#include <ESPmDNS.h>

if (MDNS.begin("signalk")) {
  MDNS.addService("signalk-http", "tcp", 80);
  MDNS.addService("signalk-ws", "tcp", 80);
  Serial.println("mDNS: http://signalk.local");
}
```

### Custom Baud Rate
```cpp
#define NMEA_BAUD 38400  // Για πιο γρήγορα devices
```

## 🐛 Troubleshooting

### WiFi δεν συνδέεται

1. Κράτα πατημένο το boot button για 3 δευτερόλεπτα
2. Reset WiFi settings (θα δημιουργηθεί ξανά το AP)

### Δεν βλέπω GPS data

1. Έλεγξε τα pins (RX/TX ανάποδα!)
2. Έλεγξε το baud rate (4800 για GPS, 38400 για AIS)
3. Δες το Serial Monitor: `pio device monitor`
4. Πρέπει να βλέπεις NMEA sentences: `$GPRMC,123456,...`

### WebSocket δεν συνδέεται

1. Έλεγξε το firewall
2. Δοκίμασε από browser console: `new WebSocket('ws://IP/signalk/v1/stream')`
3. Δες αν το ESP32 έχει αρκετή μνήμη: `ESP.getFreeHeap()`

### Token authentication fails

1. Έλεγξε ότι χρησιμοποιείς το σωστό format: `Bearer TOKEN`
2. Το token case-sensitive!
3. Δες τα available tokens: `/admin/tokens` με admin token

## 📱 Mobile App Integration

Το SignalK API είναι συμβατό με:

- **WilhelmSK** (iOS)
- **SignalK Mobile** (Android)
- **iNavX** (iOS/Android)
- **OpenCPN** (με SignalK plugin)

### Configuration στο Mobile App:

1. Server URL: `http://192.168.1.100`
2. WebSocket: `ws://192.168.1.100/signalk/v1/stream`
3. Authentication: Bearer token ή cookie
4. Subscribe to: `vessels.self.navigation.*`

## 📈 Performance Tips

### Για πολλά WebSocket clients:
```cpp
#define WS_MAX_QUEUED_MESSAGES 64
```

### Για μεγάλα JSON deltas:
```cpp
DynamicJsonDocument doc(8192);  // Αύξησε το size
```

### Rate limiting:
```cpp
const uint32_t DELTA_MIN_PERIOD_MS = 100;  // Μείωσε για πιο συχνά updates
```

## 🔄 OTA Updates

### Enable OTA στο platformio.ini:
```ini
upload_protocol = espota
upload_port = 192.168.1.100
```

### Upload over WiFi:
```bash
pio run --target upload --upload-port 192.168.1.100
```

## 📝 Supported NMEA Sentences

- ✅ **RMC** - Position, speed, course
- ✅ **GGA** - Position, altitude, satellites
- ✅ **VTG** - Speed and course
- ✅ **HDG** - Heading

### Προσθήκη νέων sentences:

```cpp
// Στη parseNMEASentence():
else if (msgType.endsWith("DBT") && fields.size() >= 4) {
  double depth = fields[3].toDouble();  // meters
  setPathValue("environment.depth.belowTransducer", depth, 
               "nmea0183.depthsounder", "m", "Depth");
}
```

## 🎯 SignalK Paths

### Navigation
- `navigation.position.latitude` (deg)
- `navigation.position.longitude` (deg)
- `navigation.speedOverGround` (m/s)
- `navigation.courseOverGroundTrue` (rad)
- `navigation.headingMagnetic` (rad)
- `navigation.gnss.altitude` (m)
- `navigation.gnss.satellitesInView` (count)

### Custom Paths
Μπορείς να προσθέσεις δικά σου:
```cpp
setPathValue("electrical.batteries.house.voltage", 12.6, 
             "manual", "V", "House battery voltage");
```

## 📚 Resources

- [SignalK Specification](https://signalk.org/specification/)
- [SignalK REST API](https://signalk.org/specification/1.7.0/doc/rest_api.html)
- [NMEA0183 Reference](https://www.tronico.fi/OH6NT/docs/NMEA0183.pdf)
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)

## 🤝 Contributing

Contributions welcome! Ιδέες για βελτιώσεις:

- [ ] NMEA2000 support (CAN bus)
- [ ] Data logging to SD card
- [ ] Historical data API
- [ ] Alarm/notification system
- [ ] Multi-vessel support
- [ ] Chart plotter integration

## 📄 License

MIT License - Free to use and modify!

---

**Made with ⚓ for the marine community**