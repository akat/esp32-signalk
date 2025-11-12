# ESP32 SignalK Gateway

A complete marine data acquisition and distribution system based on ESP32, designed for the **LILYGO TTGO T-CAN485** board. This project creates a powerful SignalK server that bridges NMEA 0183, NMEA 2000, GPS, and I2C sensors to WiFi-enabled marine apps.

## Features

### Core Functionality
- **SignalK HTTP/WebSocket Server** (Port 3000)
- **WiFi Access Point & Client Mode** with WiFiManager
- **Token-Based Authentication** with admin approval UI
- **Push Notifications** via Expo (iOS/Android)
- **Real-time Alarms**: Geofence, Wind, Depth
- **mDNS Discovery** (signalk.local)
- **Web Dashboard** for monitoring and configuration

### Data Sources

#### 1. NMEA 2000 (CAN Bus)
- **Supported PGNs**:
  - 129025: Position (Lat/Lon)
  - 129026: COG & SOG
  - 130306: Wind Speed & Direction
  - 128267: Water Depth
  - 130310: Environmental Data (Temperature, Pressure)
- **Mode**: Listen-only (won't interfere with existing network)
- **Auto-alarm integration** for wind and depth

#### 2. GPS Module (NMEA 0183 via UART)
- Supports GGA, RMC, VTG, HDT sentences
- Automatic position, speed, and course updates
- Configurable baud rate (default 9600)

#### 3. NMEA 0183 Instruments (UART)
- Standard marine instruments connection
- Supports: GPS, Wind, Depth, Speed, Heading, Temperature
- 4800 baud (standard marine)

#### 4. I2C Environmental Sensors
- **BME280**: Temperature, Barometric Pressure, Humidity
- Auto-detection at I2C addresses 0x76 and 0x77
- Inside cabin environmental monitoring

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
- ESP32 MCU with 4MB Flash
- Built-in CAN transceiver (SN65HVD231)
- Built-in RS485 transceiver (MAX13487EESA+)
- SD card slot
- USB-C programming interface
- 5-12V power input (perfect for marine 12V systems)

### Optional Modules
- **GPS Module**: Any NMEA 0183 UART GPS (NEO-6M, NEO-M8N, etc.)
- **BME280 Sensor**: I2C environmental sensor
- **12V Power Supply**: For marine installations

## Pin Configuration

### LILYGO T-CAN485 Pinout

```
┌─────────────────────────────────────────┐
│      LILYGO TTGO T-CAN485 ESP32        │
├─────────────────────────────────────────┤
│                                         │
│  CAN Bus (Built-in)                     │
│  ├─ TX:  GPIO 5                         │
│  └─ RX:  GPIO 35                        │
│                                         │
│  NMEA 0183 UART (Serial1)               │
│  ├─ RX:  GPIO 16                        │
│  └─ TX:  GPIO 17                        │
│                                         │
│  GPS Module (Serial2)                   │
│  ├─ RX:  GPIO 25                        │
│  └─ TX:  GPIO 26                        │
│                                         │
│  I2C Sensors                            │
│  ├─ SDA: GPIO 18                        │
│  └─ SCL: GPIO 19                        │
│                                         │
│  RS485 (Built-in)                       │
│  ├─ RX:  GPIO 21                        │
│  ├─ TX:  GPIO 22                        │
│  └─ DE:  GPIO 17                        │
│                                         │
│  RGB LED:    GPIO 4                     │
│  Power:      5-12V DC                   │
│                                         │
└─────────────────────────────────────────┘
```

## Connection Diagrams

### GPS Module Connection

```
GPS Module (NEO-6M/NEO-M8N)
┌─────────────┐
│   GPS       │
│             │
│  VCC ●──────┼──── 3.3V
│  GND ●──────┼──── GND
│  TX  ●──────┼──── GPIO 25 (RX)
│  RX  ●──────┼──── GPIO 26 (TX) [Optional]
│             │
└─────────────┘
```

### BME280 Sensor Connection

```
BME280 Sensor
┌─────────────┐
│   BME280    │
│             │
│  VCC ●──────┼──── 3.3V
│  GND ●──────┼──── GND
│  SDA ●──────┼──── GPIO 18
│  SCL ●──────┼──── GPIO 19
│             │
└─────────────┘
```

### NMEA 2000 Connection

```
NMEA 2000 Network
┌─────────────────────────┐
│  Marine Electronics     │
│  (Chartplotter, MFD,    │
│   Instruments, etc.)    │
│                         │
│    CANH ●───────●       │
│    CANL ●───────●       │
│                         │
└─────────────┬───────────┘
              │
        120Ω Termination
              │
┌─────────────┴───────────┐
│  T-CAN485 Board         │
│  (Built-in CAN Bus)     │
└─────────────────────────┘
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
   │                └──────────────────────────┘                │
   │                                                            │
   │ NMEA 2000 (CAN)                                   GPS (UART)
   │                                                            │
┌──▼───────────────┐                              ┌────────────▼─────┐
│ Marine Network   │                              │  NEO-M8N GPS     │
│ - Chartplotter   │                              │  Module          │
│ - Wind Sensor    │                              └──────────────────┘
│ - Depth Sounder  │
│ - Engine Data    │                  ┌─────────────────────┐
└──────────────────┘                  │  BME280 Sensor      │
                                      │  (Temperature,      │
┌──────────────────┐                  │   Pressure,         │
│ NMEA 0183        │                  │   Humidity)         │
│ Instruments      ├──────────────────┤                     │
│ (4800 baud)      │     Serial1      └─────────────────────┘
└──────────────────┘
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
   pio device monitor
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

## Configuration

### WiFi Settings
- **AP SSID**: `ESP32-SignalK` (change in main.cpp `AP_SSID`)
- **AP Password**: `signalk123` (change in main.cpp `AP_PASSWORD`)
- **Portal**: Always available at `http://192.168.4.1`

### Serial Port Settings
```cpp
// NMEA 0183 UART (Serial1)
#define NMEA_RX 16
#define NMEA_TX 17
#define NMEA_BAUD 4800

// GPS Module (Serial2)
#define GPS_RX 25
#define GPS_TX 26
#define GPS_BAUD 9600
```

### CAN Bus Settings
```cpp
// NMEA 2000 CAN Bus
#define CAN_TX 5
#define CAN_RX 35
```

### I2C Settings
```cpp
// I2C Sensors
#define I2C_SDA 18
#define I2C_SCL 19
```

### Alarm Thresholds
```cpp
// Default values (configurable via API)
Geofence radius: 100 meters
Wind threshold: 20 knots
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

## Alarm Configuration

### Geofence Setup

**Set Anchor Position:**
```bash
curl -X POST http://signalk.local:3000/api/geofence/set-anchor
```

**Enable Geofence:**
```bash
curl -X POST http://signalk.local:3000/api/geofence/enable \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "radius": 100}'
```

### Wind Alarm Setup

Wind alarms are configured in the code:
```cpp
windAlarm.threshold = 20.0;  // knots
windAlarm.enabled = true;
```

### Depth Alarm Setup

Depth alarms are configured in the code:
```cpp
depthAlarm.threshold = 2.0;  // meters
depthAlarm.enabled = true;
```

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

- `nmea0183.GPS`: Serial1 NMEA 0183 GPS
- `nmea0183.Instrument`: Serial1 NMEA 0183 instruments
- `nmea2000.can`: NMEA 2000 CAN bus
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

### CAN Bus Not Working

**Problem**: No NMEA 2000 data received

**Solution**:
1. Check CAN bus termination (120Ω required)
2. Verify CAN_H and CAN_L connections
3. Check serial monitor for "NMEA2000 initialized successfully"
4. Verify NMEA 2000 network is powered and active

### GPS Not Working

**Problem**: No position data

**Solution**:
1. Check GPS module power (3.3V)
2. Verify GPS TX → ESP32 GPIO 25 connection
3. Ensure GPS has clear sky view
4. Check serial monitor for GPS sentences
5. Wait 30-60 seconds for GPS fix

### Push Notifications Not Working

**Problem**: Mobile app not receiving notifications

**Solution**:
1. Verify Expo token registration: Check serial monitor
2. Ensure mobile app has notification permissions
3. Test alarm trigger manually
4. Check WiFi connectivity
5. Verify 3-second rate limiting isn't blocking messages

### Serial Monitor Showing Errors

**Error**: `WiFi connection lost`
**Solution**: WiFi auto-reconnects every 5 seconds. Check router signal strength.

**Error**: `NMEA2000 initialization failed`
**Solution**: Check CAN bus wiring and termination resistors.

**Error**: `No BME280 sensor detected`
**Solution**: Normal if sensor not connected. Verify I2C wiring if sensor is present.

## Performance

- **RAM Usage**: ~15.9% (52KB / 328KB)
- **Flash Usage**: ~41.9% (1.32MB / 3.14MB)
- **WiFi Reconnect**: Every 5 seconds if disconnected
- **Sensor Update Rate**: 2 seconds (I2C sensors)
- **WebSocket Delta Rate**: 500ms minimum
- **Push Notification Rate**: 3 seconds per alarm type

## Development

### Building from Source

```bash
# Install PlatformIO
pip install platformio

# Clone repository
git clone [your-repo-url]
cd esp32-signalk

# Install dependencies
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
│   └── main.cpp           # Main application code
├── platformio.ini         # PlatformIO configuration
└── README.md             # This file
```

### Library Dependencies

- WiFiManager 2.0.17
- ESPAsyncWebServer
- AsyncTCP
- ArduinoJson 6.21.5
- NMEA2000-library
- NMEA2000_esp32
- Adafruit BME280 Library
- Adafruit Unified Sensor
- Adafruit BusIO

### Partition Scheme

Uses `huge_app.csv` partition table:
- **App**: 3MB (OTA support)
- **NVS**: 20KB (settings storage)
- **No SPIFFS**: All storage for application

## Future Enhancements

- [ ] RS485 NMEA 0183 support (requires additional UART)
- [ ] SD card data logging
- [ ] More NMEA 2000 PGNs (engine, rudder, AIS)
- [ ] Compass/IMU support (heel, pitch, roll)
- [ ] NMEA 0183 output (autopilot integration)
- [ ] Web-based alarm configuration
- [ ] Historical data graphs
- [ ] AIS target tracking

## License

This project is open source. Feel free to use, modify, and distribute.

## Credits

- **NMEA2000 Library**: Timo Lappalainen
- **SignalK Protocol**: SignalK Project
- **ESP32 Arduino Core**: Espressif Systems
- **WiFiManager**: tzapu

## Support

For issues, questions, or contributions:
- Check serial monitor output first
- Review this README thoroughly
- Open an issue on GitHub

## Safety Notice

⚠️ **This system is for informational purposes only. Always use official marine navigation equipment for critical safety decisions. This device should supplement, not replace, proper marine navigation instruments.**

---

**Built with ❤️ for the marine community**

*Last Updated: 2025*
