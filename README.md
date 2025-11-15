# ESP32 SignalK Gateway

A complete marine data acquisition and distribution system based on ESP32, designed for the **LILYGO TTGO T-CAN485** board. This project creates a powerful SignalK server that bridges NMEA 0183, NMEA 2000, GPS, and I2C sensors to WiFi-enabled marine apps.

## Features

### Core Functionality
- **SignalK HTTP/WebSocket Server** (Port 3000)
- **NMEA 0183 TCP Server** (Port 10110) - NEW! Broadcast NMEA sentences to marine apps
- **WiFi Access Point & Client Mode** with WiFiManager
- **Token-Based Authentication** with admin approval UI
- **Push Notifications** via Expo (iOS/Android)
- **Real-time Alarms**: Geofence, Wind, Depth
- **mDNS Discovery** (signalk.local)
- **Web Dashboard** for monitoring and configuration

### Data Sources

#### 1. NMEA 2000 (CAN Bus)
- **Hardware**: Built-in CAN transceiver (SN65HVD231) on GPIO 27/26
- **Connection**: Use LEFT green terminal block (H/L)
- **Supported PGNs**:
  - 129025: Position (Lat/Lon)
  - 129026: COG & SOG
  - 130306: Wind Speed & Direction
  - 128267: Water Depth
  - 130310: Environmental Data (Temperature, Pressure)
- **Mode**: Listen-only (won't interfere with existing network)
- **Auto-alarm integration** for wind and depth
- **📖 See [NMEA2000-GUIDE.md](NMEA2000-GUIDE.md) for detailed connection instructions**

#### 2. NMEA 0183 Instruments (RS485)
- **Hardware**: Built-in RS485 transceiver (MAX13487EESA+) on GPIO 21/22
- **Connection**: Use RIGHT green terminal block (A/B) **OR** GPIO header pins (32/33)
- **Configurable**: Switch between RS485 terminals or GPIO header via `#define USE_RS485_FOR_NMEA`
- **Supports**: Marine instruments (depth sounders, wind sensors, AIS, etc.)
- **Baud**: 4800 bps (standard marine) or 38400 bps (high-speed)
- **📖 See [RS485-GUIDE.md](RS485-GUIDE.md) for RS485 terminal block usage**

#### 3. GPS Module (NMEA 0183 via UART)
- **Connection**: Dedicated Serial2 on GPIO 25 (RX) / 18 (TX)
- **Supports**: Standard GPS modules (NEO-6M, NEO-M8N, etc.)
- **Sentences**: GGA, RMC, VTG, HDT
- **Baud**: 9600 bps (configurable)
- **Automatic**: Position, speed, and course updates

#### 4. I2C Environmental Sensors
- **Hardware**: I2C bus on GPIO 5 (SDA) / 32 (SCL)
- **BME280**: Temperature, Barometric Pressure, Humidity
- **Auto-detection**: Addresses 0x76 and 0x77
- **Use Case**: Inside cabin environmental monitoring

### Alarms & Notifications

#### Geofence Alarm
- Set anchor position and radius
- Continuous notifications while outside boundary (3-second intervals)
- Visual and push notification alerts

#### Wind Alarm
- Configurable wind speed threshold (knots)
- True wind speed monitoring
- Push notifications on threshold breach

#### Depth Alarm
- Configurable minimum depth (meters)
- Push notifications on shallow water
- Auto-clear when depth returns to normal

#### Push Notifications
- Expo push notification service integration
- Multi-device support
- Custom sound: `geofence_alarm.wav`
- 3-second rate limiting
- Channel: `geofence-alarms`

## Hardware Requirements

### Recommended Board
**LILYGO TTGO T-CAN485 ESP32**
- ESP32-WROOM-32 MCU with 4MB Flash
- Built-in CAN transceiver (SN65HVD231) for NMEA 2000
- Built-in RS485 transceiver (MAX13487EESA+) for NMEA 0183
- 12-pin GPIO header for external connections
- SD card slot (not used in this firmware)
- USB-C programming interface
- 5-12V power input (perfect for marine 12V systems)
- Onboard RGB LED (WS2812)

**Purchase**: Available on [LILYGO Store](https://www.lilygo.cc/) or AliExpress

### Optional Modules
- **GPS Module**: Any NMEA 0183 UART GPS (NEO-6M, NEO-M8N, etc.) - ~$10
- **BME280 Sensor**: I2C environmental sensor - ~$5
- **12V Power Supply**: For marine installations
- **NMEA 2000 T-Connector**: For connecting to existing N2K network
- **NMEA 2000 Drop Cable**: For network connection

## Pin Configuration

### LILYGO T-CAN485 Pinout

```
┌─────────────────────────────────────────┐
│      LILYGO TTGO T-CAN485 ESP32        │
├─────────────────────────────────────────┤
│                                         │
│  📍 NMEA 2000 CAN Bus (Built-in)       │
│  ├─ TX:    GPIO 27  (Hardware fixed)    │
│  ├─ RX:    GPIO 26  (Hardware fixed)    │
│  └─ Term:  LEFT terminal block (H/L)    │
│                                         │
│  📍 NMEA 0183 RS485 (Configurable)     │
│     OPTION A: Built-in RS485            │
│  ├─ RX:    GPIO 21  (RS485 terminal)    │
│  ├─ TX:    GPIO 22  (RS485 terminal)    │
│  ├─ DE:    GPIO 17  (Driver Enable)     │
│  ├─ EN:    GPIO 19  (Chip Enable)       │
│  └─ Term:  RIGHT terminal block (A/B)   │
│                                         │
│     OPTION B: GPIO Header UART          │
│  ├─ RX:    GPIO 32  (Pin 3)             │
│  ├─ TX:    GPIO 33  (Pin 4)             │
│  └─ Use:   When RS485 not needed        │
│                                         │
│  📍 GPS Module (Serial2)                │
│  ├─ RX:    GPIO 25  (Pin 5)             │
│  ├─ TX:    GPIO 18  (Pin 6)             │
│  └─ Power: 3.3V (Pin 1), GND (Pin 2)    │
│                                         │
│  📍 I2C Sensors                         │
│  ├─ SDA:   GPIO 5   (Pin 7)             │
│  ├─ SCL:   GPIO 32  (Pin 3)             │
│  └─ Note:  SCL shares with NMEA RX      │
│            (OK when using RS485 mode)   │
│                                         │
│  📍 Other Hardware                      │
│  ├─ RGB LED:     GPIO 4                 │
│  ├─ Power En:    GPIO 16                │
│  ├─ CAN SE:      GPIO 23                │
│  └─ SD Card:     GPIO 2,13,14,15        │
│                                         │
│  📍 Power                               │
│  ├─ Input:  5-12V DC or USB-C           │
│  ├─ Output: 3.3V / 5V regulated         │
│  └─ Current: ~200mA typical             │
│                                         │
└─────────────────────────────────────────┘

📖 For detailed pin identification, see PINOUT.md
📖 For RS485 terminal usage, see RS485-GUIDE.md
📖 For NMEA 2000 wiring, see NMEA2000-GUIDE.md
```

### Connection Mode Selection

**Choose your NMEA 0183 connection method** in `src/main.cpp` (line ~57):

```cpp
#define USE_RS485_FOR_NMEA    // ← Leave uncommented to use RS485 terminal blocks
// #define USE_RS485_FOR_NMEA // ← Comment out to use GPIO header (pins 32/33)
```

**When to use RS485 terminals:**
- Modern marine instruments with RS485 output
- Long cable runs (up to 1200m)
- Multiple devices on same bus
- Noisy marine electrical environment

**When to use GPIO header:**
- Simple GPS modules (TTL level)
- Short cable runs (<15m)
- Single device connection
- Testing on breadboard

## Connection Diagrams

### NMEA 2000 (CAN Bus) Connection

```
NMEA 2000 Backbone
       │
       ├──┬── T-Connector (M12 5-pin female)
       │  │
       │  │   Drop Cable
       │  │   (Max 6m)
       │  │
       │  ├─ WHITE (CAN_H) ──────→ H │ T-CAN485
       │  ├─ BLUE  (CAN_L) ──────→ L │ LEFT Terminal
       │  ├─ RED   (12V)   ──────→ 12V Power Input
       │  └─ BLACK (GND)   ──────→ GND
       │
       └─── (Continues to other devices)

Add 120Ω terminator at EACH END of backbone
```

**📖 Full details in [NMEA2000-GUIDE.md](NMEA2000-GUIDE.md)**

### NMEA 0183 via RS485 Terminal Blocks

```
Marine Instrument          T-CAN485
(RS485 Output)             RIGHT Terminal Block
─────────────────────────────────────────────
  A / Data+ / TX+  ──────→  A (RS485+)
  B / Data- / TX-  ──────→  B (RS485-)
  GND              ──────→  GND
  12V (if needed)  ──────→  12V Input
```

**📖 Full details in [RS485-GUIDE.md](RS485-GUIDE.md)**

### GPS Module Connection (GPIO Header)

```
GPS Module (NEO-6M/NEO-M8N)    T-CAN485 GPIO Header
┌─────────────┐
│   GPS       │
│             │
│  VCC ●──────┼──────────────→ 3.3V (Pin 1)
│  GND ●──────┼──────────────→ GND  (Pin 2)
│  TX  ●──────┼──────────────→ GPIO 25 (Pin 5) - RX
│  RX  ●──────┼──────────────→ GPIO 18 (Pin 6) - TX [Optional]
│             │
└─────────────┘
```

### BME280 Sensor Connection (GPIO Header)

```
BME280 Sensor              T-CAN485 GPIO Header
┌─────────────┐
│   BME280    │
│             │
│  VCC ●──────┼──────────────→ 3.3V (Pin 1)
│  GND ●──────┼──────────────→ GND  (Pin 2)
│  SDA ●──────┼──────────────→ GPIO 5  (Pin 7)
│  SCL ●──────┼──────────────→ GPIO 32 (Pin 3)
│             │
└─────────────┘

Note: GPIO 32 is shared with NMEA RX when using GPIO header mode.
      Use RS485 mode for NMEA if connecting BME280.
```

### Complete System Diagram

```
                    ┌──────────────────────────┐
                    │   12V Marine Power       │
                    │   (or 5V USB)            │
                    └────────────┬─────────────┘
                                 │
                    ┌────────────▼─────────────┐
                    │                          │
   ┌────────────────┤  LILYGO T-CAN485 ESP32  ├────────────────┐
   │                │                          │                │
   │   LEFT         └──────────────────────────┘     RIGHT      │
   │   Terminal                                      Terminal   │
   │   (CAN H/L)                                     (RS485 A/B)│
   │                                                             │
   │ NMEA 2000 (CAN)                                NMEA 0183   │
   │                                                (RS485)     │
┌──▼───────────────┐                              ┌────────────▼─────┐
│ Marine Network   │                              │  Wind/Depth/AIS  │
│ - Chartplotter   │                              │  Instruments     │
│ - MFD            │                              │  (RS485 Output)  │
│ - Wind Sensor    │                              └──────────────────┘
│ - Depth Sounder  │
│ - Engine Data    │                  ┌─────────────────────┐
└──────────────────┘                  │  GPIO Header        │
                                      │  12-pin connector   │
                                      │                     │
                                      │  - GPS Module       │
                                      │  - BME280 Sensor    │
                                      └─────────────────────┘
                                               │
                                               │ WiFi
                    ┌──────────────────────────▼──────────┐
                    │                                     │
              ┌─────▼──────┐                  ┌──────────▼─────┐
              │  6pack App │                  │  Web Dashboard │
              │  (iOS/      │                  │  Admin Panel   │
              │   Android)  │                  │  Port 3000     │
              └────────────┘                  └────────────────┘
```

## Software Installation

### Prerequisites
- [PlatformIO](https://platformio.org/) IDE or CLI
- USB cable (USB-C for T-CAN485)
- (Optional) 6pack mobile app for iOS/Android

### Building & Uploading

1. **Clone or download this project**

2. **Open in PlatformIO**
   ```bash
   cd esp32-signalk
   pio run
   ```

3. **Upload to ESP32**
   ```bash
   pio run --target upload
   ```

4. **Monitor serial output**
   ```bash
   pio device monitor --baud 115200
   ```

### First Boot Configuration

1. **Connect to WiFi AP**
   - SSID: `ESP32-SignalK`
   - Password: `signalk123`

2. **Configure WiFi**
   - Open browser: `http://192.168.4.1`
   - Select your WiFi network
   - Enter password
   - Click "Save"

3. **Access SignalK Server**
   - Find IP address in serial monitor or use `signalk.local`
   - Open browser: `http://signalk.local:3000` or `http://[IP]:3000`

### Verify Hardware Initialization

Check serial monitor for successful initialization:

```
=== ESP32 SignalK Server ===

Starting NMEA UART...
NMEA0183 via RS485 started on terminal blocks A/B
Using built-in RS485 transceiver (GPIO 21/22)

Starting GPS module...
GPS UART started on pins RX:25 TX:18

Initializing NMEA2000...
CAN device ready
NMEA2000 initialized successfully

Initializing I2C sensors...
BME280 sensor found!  (or "No BME280 sensor detected")

=== SIGNALK SERVER READY ===
```

## API Endpoints

### Discovery
```
GET /signalk
Returns: Server information and WebSocket URL
```

### SignalK REST API
```
GET  /signalk/v1/api/vessels/self
GET  /signalk/v1/api/vessels/self/navigation/position
PUT  /signalk/v1/api/vessels/self/* (requires token)
```

### WebSocket Stream
```
WS /signalk/v1/stream
Subscribe to real-time SignalK deltas
```

### Authentication
```
POST /signalk/v1/access/requests
Body: {"clientId": "...", "description": "..."}
Returns: {"requestId": "...", "state": "PENDING"}

GET /signalk/v1/access/requests/{requestId}
Returns: {"state": "COMPLETED", "token": "..."}
```

### Admin Panel
```
GET  /admin
Admin UI for token approval/denial

POST /api/admin/approve/{requestId}
POST /api/admin/deny/{requestId}
POST /api/admin/revoke/{token}
```

### Geofence Control
```
POST /api/geofence/set-anchor
Body: {Position from current GPS}

POST /api/geofence/enable
Body: {"enabled": true, "radius": 100}

POST /api/geofence/disable
```

### Push Notifications
```
POST /plugins/signalk-node-red/redApi/register-expo-token
Body: {"token": "ExponentPushToken[...]"}
```

### NMEA 0183 TCP Server

**NEW FEATURE**: The ESP32 now broadcasts NMEA 0183 sentences via TCP on port 10110, making it compatible with any marine navigation software that supports TCP NMEA input (OpenCPN, iNavX, Navionics, etc.).

#### Features
- **Port**: 10110 (standard NMEA 0183 TCP port)
- **Protocol**: TCP Server (supports multiple simultaneous clients)
- **Max Clients**: 8 simultaneous connections
- **Auto-conversion**: NMEA 2000 data is automatically converted to NMEA 0183 sentences
- **Supported Sentences**:
  - `$GPGGA` - GPS Fix Data (position, satellites, altitude) - 1Hz
  - `$GPGLL` - Geographic Position (lat/lon) - 1Hz
  - `$GPVTG` - Track Made Good and Ground Speed - 1Hz
  - `$GPRMC` - Recommended Minimum Navigation Information - 1Hz
  - `$WIMWV` - Wind Speed and Angle - 5Hz
  - `$SDDPT` - Depth Below Transducer - 2Hz
  - `$YXMTW` - Water Temperature - 0.5Hz

#### Usage

**Connect with marine apps:**
```
Connection Type: TCP
Address: [ESP32-IP or signalk.local]
Port: 10110
```

**Test with command line:**
```bash
# Windows
telnet [ESP32-IP] 10110

# Linux/Mac
nc [ESP32-IP] 10110

# Or use netcat
ncat [ESP32-IP] 10110
```

**Example output:**
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,1.0,545.4,M,0.0,M,,*47
$GPGLL,4807.038,N,01131.000,E,123519.000,A,A*5C
$GPVTG,84.4,T,,M,22.4,N,41.5,K,A*2F
$GPRMC,123519.000,A,4807.038,N,01131.000,E,22.40,84.40,150125,,,A*6A
$WIMWV,45.0,T,12.50,M,A*2E
$SDDPT,5.20,0.00*5B
$YXMTW,18.5,C*1D
```

#### Compatible Applications
- **OpenCPN**: Add connection → Network → TCP, address: [ESP32-IP], port: 10110
- **iNavX** (iOS): Settings → NMEA → TCP Client → [ESP32-IP]:10110
- **Navionics** (iOS/Android): Settings → Connect → TCP → [ESP32-IP]:10110
- **Coastal Explorer**: Connections → Add → TCP Client → [ESP32-IP]:10110
- **Any app supporting TCP NMEA 0183 input**

#### Performance
- **Latency**: <50ms end-to-end (NMEA2000 → TCP)
- **CPU Impact**: ~3% additional load
- **Memory**: ~3-4KB RAM for 8 clients
- **Rate Limiting**: Smart throttling prevents data spam
  - Position: 1Hz (once per second)
  - Wind: 5Hz (5 times per second)
  - Depth: 2Hz (twice per second)

#### Client Management
- **Auto-disconnect**: Inactive clients timeout after 30 seconds
- **Reconnect**: Clients can reconnect automatically
- **No authentication**: Open access for marine apps (configure firewall if needed)

## Configuration

### Choosing NMEA 0183 Connection Method

Edit `src/main.cpp` around line 57:

```cpp
// Uncomment to use RS485 terminal blocks (A/B)
#define USE_RS485_FOR_NMEA

// Comment out to use GPIO header pins (32/33)
// #define USE_RS485_FOR_NMEA
```

After changing, rebuild and upload:
```bash
pio run --target upload
```

### Pin Definitions (Advanced)

```cpp
// NMEA 0183 RS485 Mode
#ifdef USE_RS485_FOR_NMEA
  #define NMEA_RX 21                 // RS485 terminal block
  #define NMEA_TX 22
  #define NMEA_DE 17                 // Driver Enable
  #define NMEA_DE_ENABLE 19          // Chip Enable
  #define NMEA_BAUD 4800
#else
  // NMEA 0183 GPIO Header Mode
  #define NMEA_RX 32                 // GPIO header pin 3
  #define NMEA_TX 33                 // GPIO header pin 4
  #define NMEA_BAUD 4800
#endif

// GPS Module (Serial2)
#define GPS_RX 25                  // GPIO header pin 5
#define GPS_TX 18                  // GPIO header pin 6
#define GPS_BAUD 9600

// CAN Bus (NMEA 2000) - Hardware fixed, DON'T CHANGE
#define CAN_TX 27
#define CAN_RX 26

// I2C Sensors
#define I2C_SDA 5                  // GPIO header pin 7
#define I2C_SCL 32                 // GPIO header pin 3 (shared with NMEA RX)
```

### Alarm Thresholds

Default values (configurable via API):
```cpp
Geofence radius: 40 meters
Wind threshold: 40 knots
Depth threshold: 2 meters
```

## Mobile App Setup

### 6pack App Configuration

1. **Install 6pack app** (iOS/Android)

2. **Add Server**
   - Server: `http://signalk.local:3000` or `http://[IP]:3000`
   - Leave token blank initially

3. **Request Access Token**
   - App will automatically request access
   - Check serial monitor for request ID
   - Open admin panel: `http://signalk.local:3000/admin`
   - Approve the request
   - App will receive token automatically

4. **Register for Push Notifications**
   - App automatically registers Expo push token
   - Notifications enabled for all alarms

## SignalK Paths

### Navigation
```
navigation.position               Vessel position (lat/lon)
navigation.speedOverGround        Speed over ground (m/s)
navigation.courseOverGroundTrue   Course over ground (radians)
navigation.headingTrue            True heading (radians)
```

### Environment
```
environment.wind.speedTrue        True wind speed (m/s)
environment.wind.angleTrueWater   True wind angle (radians)
environment.depth.belowTransducer Water depth (meters)
environment.water.temperature     Water temperature (K)
environment.outside.temperature   Outside air temperature (K)
environment.outside.pressure      Atmospheric pressure (Pa)
environment.inside.temperature    Inside cabin temperature (K)
environment.inside.pressure       Inside cabin pressure (Pa)
environment.inside.humidity       Inside relative humidity (0-1)
```

### Notifications
```
notifications.geofence.exit       Geofence alarm state
notifications.wind.alarm          Wind alarm state
notifications.depth.alarm         Depth alarm state
```

## Data Sources

Each SignalK value includes a `$source` field indicating where the data came from:

- `nmea0183.Serial1`: NMEA 0183 via RS485 or GPIO header
- `nmea2000.25`: NMEA 2000 CAN bus (device address 25)
- `gps.serial2`: GPS module on Serial2
- `i2c.bme280`: BME280 I2C sensor

## Troubleshooting

### WiFi Connection Issues

**Problem**: Can't connect to WiFi network

**Solution**:
1. Connect to AP: `ESP32-SignalK` / `signalk123`
2. Open `http://192.168.4.1`
3. Reconfigure WiFi credentials
4. Check serial monitor for connection status

### NMEA 2000 Not Working

**Problem**: No NMEA 2000 data received

**Solution**:
1. Check CAN bus termination (120Ω at BOTH ends of backbone)
2. Verify WHITE → H, BLUE → L (not swapped)
3. Check serial monitor for "NMEA2000 initialized successfully"
4. Verify NMEA 2000 network is powered (12V present)
5. Use multimeter: H and L should each read ~2.5V relative to ground
6. **📖 See [NMEA2000-GUIDE.md](NMEA2000-GUIDE.md) troubleshooting section**

### RS485 NMEA 0183 Not Working

**Problem**: No data from RS485 instruments

**Solution**:
1. Verify `#define USE_RS485_FOR_NMEA` is uncommented in main.cpp
2. Check A → A, B → B (polarity matters!)
3. Verify instruments are powered and transmitting
4. Try swapping A and B if polarity is unclear
5. Check baud rate matches instrument (4800 or 38400)
6. **📖 See [RS485-GUIDE.md](RS485-GUIDE.md) troubleshooting section**

### GPS Not Working

**Problem**: No position data

**Solution**:
1. Check GPS module power (3.3V on Pin 1)
2. Verify GPS TX → ESP32 GPIO 25 connection
3. Ensure GPS has clear sky view (outdoor or window)
4. Check serial monitor for GPS sentences
5. Wait 30-60 seconds for initial GPS fix (cold start)
6. **📖 See [PINOUT.md](PINOUT.md) for GPIO header pinout**

### I2C Sensor Not Detected

**Problem**: "No BME280 sensor detected"

**Solution**:
1. Normal if sensor not connected (system works without it)
2. If sensor connected, verify I2C wiring:
   - SDA → GPIO 5 (Pin 7)
   - SCL → GPIO 32 (Pin 3)
   - VCC → 3.3V (Pin 1)
   - GND → GND (Pin 2)
3. Check I2C address (0x76 or 0x77)
4. Verify sensor not damaged with I2C scanner

### Push Notifications Not Working

**Problem**: Mobile app not receiving notifications

**Solution**:
1. Verify Expo token registration in serial monitor
2. Ensure mobile app has notification permissions
3. Test alarm trigger manually
4. Check WiFi connectivity
5. Verify 3-second rate limiting isn't blocking messages

### Serial Monitor Errors

**Error**: `WiFi connection lost`
**Solution**: WiFi auto-reconnects every 5 seconds. Check router signal strength.

**Error**: `NMEA2000 initialization failed`
**Solution**: Check CAN bus wiring and termination resistors (120Ω).

**Error**: `CAN: Bus-off state`
**Solution**: Too many CAN errors. Check termination, verify no shorts between H and L.

**Error**: `No BME280 sensor detected`
**Solution**: Normal if sensor not connected. Check I2C wiring if sensor is present.

## Performance

- **RAM Usage**: ~16.0% (52.5KB / 328KB)
- **Flash Usage**: ~42.3% (1.33MB / 3.14MB)
- **WiFi Reconnect**: Every 5 seconds if disconnected
- **Sensor Update Rate**: 2 seconds (I2C sensors)
- **NMEA2000 Processing**: Continuous (250 kbps CAN bus)
- **NMEA 0183 TCP**: <50ms latency, supports 8 clients
- **WebSocket Delta Rate**: 500ms minimum
- **Push Notification Rate**: 3 seconds per alarm type

## Documentation

- **[README.md](README.md)** - This file (overview and getting started)
- **[PINOUT.md](PINOUT.md)** - Detailed GPIO pin identification and usage
- **[RS485-GUIDE.md](RS485-GUIDE.md)** - RS485 terminal block connection guide
- **[NMEA2000-GUIDE.md](NMEA2000-GUIDE.md)** - Complete NMEA 2000 installation guide

## Development

### Building from Source

```bash
# Install PlatformIO
pip install platformio

# Clone repository
git clone [your-repo-url]
cd esp32-signalk

# Install dependencies (automatic)
pio lib install

# Build
pio run

# Upload
pio run --target upload

# Monitor
pio device monitor --baud 115200
```

### Project Structure

```
esp32-signalk/
├── src/
│   ├── main.cpp                      # Main application entry point
│   ├── config.h                      # Configuration and pin definitions
│   ├── types.h                       # Type definitions
│   ├── api/                          # API handlers and routes
│   │   ├── handlers.cpp/h           # HTTP request handlers
│   │   ├── routes.cpp/h             # Route configuration
│   │   └── security.cpp/h           # Authentication & authorization
│   ├── hardware/                     # Hardware interfaces
│   │   ├── nmea0183.cpp/h           # NMEA 0183 parsing
│   │   ├── nmea2000.cpp/h           # NMEA 2000 CAN handlers
│   │   └── sensors.cpp/h            # I2C sensor interface
│   ├── services/                     # Background services
│   │   ├── alarms.cpp/h             # Alarm management
│   │   ├── expo_push.cpp/h          # Push notifications
│   │   ├── nmea0183_tcp.cpp/h       # NEW! TCP NMEA server
│   │   ├── storage.cpp/h            # Persistent storage
│   │   └── websocket.cpp/h          # WebSocket management
│   ├── signalk/                      # SignalK protocol
│   │   ├── data_store.cpp/h         # Data storage
│   │   └── globals.h                # Global variables
│   └── utils/                        # Utility functions
│       ├── conversions.cpp/h        # Unit conversions
│       ├── nmea0183_converter.cpp/h # NEW! N2K to 0183 converter
│       ├── time_utils.cpp/h         # Time handling
│       └── uuid.cpp/h               # UUID generation
├── platformio.ini                    # PlatformIO configuration
├── README.md                         # This file (overview)
├── PINOUT.md                         # GPIO pin identification guide
├── RS485-GUIDE.md                    # RS485 connection guide
└── NMEA2000-GUIDE.md                 # NMEA 2000 connection guide
```

### Library Dependencies

- WiFiManager 2.0.17 (WiFi configuration portal)
- ESPAsyncWebServer (High-performance web server)
- AsyncTCP (Async TCP library)
- ArduinoJson 6.21.5 (JSON parsing)
- NMEA2000-library (Timo Lappalainen's NMEA 2000 stack)
- NMEA2000_esp32 (ESP32 CAN driver)
- Adafruit BME280 Library (I2C sensor)
- Adafruit Unified Sensor (Sensor abstraction)
- Adafruit BusIO (I2C/SPI abstraction)

### Partition Scheme

Uses `huge_app.csv` partition table:
- **App 0**: 3MB (current firmware)
- **App 1**: 3MB (OTA updates - not yet implemented)
- **NVS**: 20KB (WiFi settings, tokens, alarms)
- **OTA Data**: 8KB (OTA metadata)
- **No SPIFFS**: All flash space dedicated to application

## Future Enhancements

- [x] RS485 NMEA 0183 support
- [x] NMEA 2000 CAN bus support
- [x] GPS module integration
- [x] I2C sensor support (BME280)
- [x] Comprehensive documentation
- [x] NMEA 0183 TCP server (port 10110) - **NEW!**
- [ ] SD card data logging
- [ ] More NMEA 2000 PGNs (engine, rudder, AIS)
- [ ] Compass/IMU support (heel, pitch, roll)
- [ ] Web-based alarm configuration UI
- [ ] Historical data graphs
- [ ] AIS target tracking
- [ ] OTA firmware updates
- [ ] NMEA 2000 transmission mode (currently listen-only)

## License

This project is open source. Feel free to use, modify, and distribute.

## Credits

- **NMEA2000 Library**: Timo Lappalainen
- **SignalK Protocol**: SignalK Project
- **ESP32 Arduino Core**: Espressif Systems
- **WiFiManager**: tzapu
- **Hardware**: LILYGO (Xinyuan Technology)

## Support

For issues, questions, or contributions:
1. Check serial monitor output first
2. Review this README and relevant guide documents
3. Check the documentation:
   - [PINOUT.md](PINOUT.md) for pin identification
   - [RS485-GUIDE.md](RS485-GUIDE.md) for RS485 connections
   - [NMEA2000-GUIDE.md](NMEA2000-GUIDE.md) for CAN bus setup
4. Open an issue on GitHub

## Safety Notice

⚠️ **This system is for informational purposes only. Always use official marine navigation equipment for critical safety decisions. This device should supplement, not replace, proper marine navigation instruments.**

**Marine Environment Considerations:**
- Use waterproof enclosures for all electronics
- Properly fuse all power connections
- Use marine-grade wire and connectors
- Follow ABYC electrical standards
- Test thoroughly before relying on data
- Maintain redundant navigation systems

---

**Built with ❤️ for the marine community**

*Last Updated: November 2025*
*Board: LILYGO TTGO T-CAN485*
*Firmware Version: 1.0*
