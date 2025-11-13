# Hardware Modules - Quick Reference

## File Structure

```
src/hardware/
├── nmea0183.h         (70 lines)   - NMEA 0183 parsing declarations
├── nmea0183.cpp       (408 lines)  - NMEA 0183 parsing implementation
├── nmea2000.h         (61 lines)   - NMEA 2000 CAN handler declarations
├── nmea2000.cpp       (159 lines)  - NMEA 2000 CAN handler implementation
├── sensors.h          (35 lines)   - I2C sensor declarations
└── sensors.cpp        (70 lines)   - I2C sensor implementation
```

## NMEA 0183 Module (Serial GPS/Instruments)

### Functions

```cpp
// Validate NMEA checksum (XOR-based)
bool validateNmeaChecksum(const String& sentence);

// Convert NMEA coordinates to decimal degrees
// Input: "5215.1234" (52°15.1234'N = 52.2520567°)
double nmeaCoordToDec(const String& coord, const String& hemisphere);

// Split NMEA sentence into fields
std::vector<String> splitNMEA(const String& sentence);

// Parse complete NMEA sentence (main entry point)
void parseNMEASentence(const String& sentence);
```

### Supported Sentence Types

| Type | Description | Fields |
|------|-------------|--------|
| RMC | Recommended Minimum | Position, Speed, Course, Date |
| GGA | GPS Fix Data | Position, Satellites, Altitude, Quality |
| GLL | Geographic Position | Latitude, Longitude |
| VTG | Track & Speed | Course, Speed |
| HDG | Heading | Magnetic heading, Deviation, Variation |
| HDM | Heading Magnetic | Magnetic heading |
| HDT | Heading True | True heading |
| VHW | Water Speed & Heading | True/Mag heading, Water speed |
| DBT | Depth | Feet, Meters, Fathoms |
| MWD | Wind Data | Direction (true/mag), Speed |
| MWV | Wind Speed & Angle | Angle, Reference (R/T), Speed, Status |
| VWD | Wind Speed & Angle True | Left/Right angle, Speed |
| VDR | Current Set & Drift | Set, Drift |
| VPW | Speed Parallel to Wind | Speed |
| WCV | Waypoint Closure Velocity | Velocity |
| XTE | Cross-Track Error | Status, Error, Direction |
| ZDA | Time & Date | Hours, Minutes, Seconds, Day, Month, Year |
| GSV | Satellites in View | Message info, Satellite count |

### Usage

```cpp
// In serial event handler
if (Serial1.available()) {
    String sentence = Serial1.readStringUntil('\n');
    parseNMEASentence(sentence);
}

// Parsed data available in gpsData global struct
Serial.printf("Lat: %.6f, Lon: %.6f\n", gpsData.lat, gpsData.lon);
```

---

## NMEA 2000 Module (CAN Bus)

### Functions

```cpp
// PGN 129025: Position (Latitude/Longitude)
void HandleN2kPosition(const tN2kMsg &N2kMsg);

// PGN 129026: Course Over Ground & Speed Over Ground
void HandleN2kCOGSOG(const tN2kMsg &N2kMsg);

// PGN 130306: Wind Speed & Angle (true/apparent)
void HandleN2kWindSpeed(const tN2kMsg &N2kMsg);

// PGN 128267: Water Depth Below Transducer
void HandleN2kWaterDepth(const tN2kMsg &N2kMsg);

// PGN 130310: Outside Environment (temp, pressure)
void HandleN2kOutsideEnvironment(const tN2kMsg &N2kMsg);

// Initialize NMEA 2000 CAN bus
void initNMEA2000();
```

### Supported PGNs

| PGN | Description | Data |
|-----|-------------|------|
| 129025 | Position | Latitude, Longitude |
| 129026 | COG & SOG | Course, Speed, Heading Reference |
| 128267 | Water Depth | Depth, Offset, Range |
| 130306 | Wind Speed & Angle | Speed, Angle, Reference (true/apparent) |
| 130310 | Environment | Water Temp, Air Temp, Atmospheric Pressure |

### Features

- Listen-only mode (no address claiming)
- Proper N2K_NA value handling
- Forward mode for debugging
- Automatic initialization with device info

### Usage

```cpp
// In setup()
initNMEA2000();

// In loop() - messages handled automatically via callbacks
NMEA2000.ParseMessages();

// Data available in gpsData and via setPathValue()
```

---

## Sensors Module (I2C)

### Functions

```cpp
// Initialize I2C and BME280 sensor
// Tries addresses 0x76 and 0x77 automatically
void initI2CSensors();

// Read sensor values (throttled to 2-second intervals)
// Updates: environment.inside.temperature (K)
//          environment.inside.pressure (Pa)
//          environment.inside.humidity (0-1)
void readI2CSensors();
```

### Supported Sensors

| Sensor | Address | Data |
|--------|---------|------|
| BME280 | 0x76 / 0x77 | Temperature, Pressure, Humidity |

### Features

- Automatic I2C address detection
- 2-second read throttling (prevents I2C saturation)
- Temperature automatically converted to Kelvin
- Humidity as relative fraction (0.0-1.0)

### Usage

```cpp
// In setup()
initI2CSensors();

// In loop()
readI2CSensors();

// Sensor availability flag
if (bmeEnabled) {
    // Sensor data is being read and published
}
```

---

## Integration Checklist

### 1. Include Headers
```cpp
#include "hardware/nmea0183.h"
#include "hardware/nmea2000.h"
#include "hardware/sensors.h"
```

### 2. Setup Function
```cpp
void setup() {
    // ... other setup code ...
    
    // Initialize CAN bus
    initNMEA2000();
    
    // Initialize I2C sensors
    initI2CSensors();
}
```

### 3. Main Loop
```cpp
void loop() {
    // ... other loop code ...
    
    // Process NMEA 2000 messages
    NMEA2000.ParseMessages();
    
    // Read I2C sensors
    readI2CSensors();
}
```

### 4. Serial Event Handler (NMEA 0183)
```cpp
void serialEvent1() {  // Serial1 = NMEA 0183
    while (Serial1.available()) {
        String sentence = Serial1.readStringUntil('\n');
        parseNMEASentence(sentence);
    }
}
```

---

## Global Variables Required

All these variables must be declared in main:

```cpp
// GPS Data struct
struct GPSData {
  double lat = NAN;
  double lon = NAN;
  double sog = NAN;
  double cog = NAN;
  double heading = NAN;
  double altitude = NAN;
  int satellites = 0;
  String fixQuality;
  String timestamp;
} gpsData;

// I2C Sensor
Adafruit_BME280 bme;
bool bmeEnabled = false;
unsigned long lastSensorRead = 0;

// CAN Bus
bool n2kEnabled = false;
```

---

## Helper Functions Required

These functions must be defined in main application:

```cpp
// Time
String iso8601Now();                    // ISO 8601 timestamp

// Conversions
double knotsToMS(double knots);        // Speed: knots -> m/s
double degToRad(double deg);           // Angle: degrees -> radians
double RadToDeg(double rad);           // Angle: radians -> degrees
double KelvinToC(double kelvin);       // Temperature: Kelvin -> Celsius

// Path Operations
void setPathValue(const String& path, double value, const String& source,
                  const String& unit, const String& description);
void setPathValue(const String& path, const String& value, const String& source,
                  const String& unit, const String& description);

// Alarms
void updateWindAlarm(double windSpeedMS);
void updateDepthAlarm(double depth);
void updateNavigationPosition(double lat, double lon, const String& source = "nmea0183.GPS");
```

---

## Troubleshooting

### NMEA 0183 not parsing
- Check serial baud rate (usually 4800 bps)
- Verify sentence format (starts with '$', ends with '\n' or '\r\n')
- Check checksum validation (warnings logged to Serial)
- Verify RX pin connection

### NMEA 2000 not working
- Confirm CAN pins are correct (TX: GPIO27, RX: GPIO26)
- Check n2kEnabled flag (Serial.println or LED)
- Monitor forward output to Serial console
- Verify CAN bus termination (120 ohm resistors)

### Sensor not detected
- Check I2C address (use I2C scanner)
- Verify SDA/SCL pin connections
- Check I2C clock speed (400 kHz typical)
- Confirm BME280 power supply

### Data not updating
- Check setPathValue() implementation
- Verify external function declarations
- Monitor Serial output for errors
- Check gpsData struct updates

---

**Module Version**: 1.0
**Extracted from**: main_original.cpp (lines 736-1117, 3281-3408, 3411-3461)
**Total Lines**: 803
**Status**: Ready for integration
