# Hardware Modules

This directory contains modular hardware interface components extracted from the main ESP32-SignalK application.

## Module Structure

### NMEA 0183 Serial Interface (`nmea0183.*`)
Parses NMEA 0183 sentences from serial GPS and marine instruments.

**Files:**
- `nmea0183.h` - Public interface declarations
- `nmea0183.cpp` - Implementation

**Main Functions:**
- `parseNMEASentence()` - Parse and process NMEA sentences
- `validateNmeaChecksum()` - Validate NMEA checksum
- `nmeaCoordToDec()` - Convert coordinates from NMEA format
- `splitNMEA()` - Split sentence into fields

**Features:**
- Support for 15+ NMEA sentence types (RMC, GGA, VTG, HDG, etc.)
- XOR checksum validation
- Coordinate conversion (DDMM.MMMM to decimal degrees)
- Automatic alarm triggering for wind/depth

### NMEA 2000 CAN Bus Interface (`nmea2000.*`)
Handles NMEA 2000 (CAN bus) protocol messages from marine electronics.

**Files:**
- `nmea2000.h` - Public interface declarations
- `nmea2000.cpp` - Implementation

**Main Functions:**
- `initNMEA2000()` - Initialize CAN interface
- `HandleN2kPosition()` - Handle position messages (PGN 129025)
- `HandleN2kCOGSOG()` - Handle course/speed messages (PGN 129026)
- `HandleN2kWindSpeed()` - Handle wind data (PGN 130306)
- `HandleN2kWaterDepth()` - Handle depth data (PGN 128267)
- `HandleN2kOutsideEnvironment()` - Handle environment data (PGN 130310)

**Features:**
- Listen-only mode (no address claiming)
- Proper N2K_NA value handling
- Product/device information configuration
- Debug forward mode for monitoring

### I2C Sensors (`sensors.*`)
Interface for I2C environmental sensors (BME280).

**Files:**
- `sensors.h` - Public interface declarations
- `sensors.cpp` - Implementation

**Main Functions:**
- `initI2CSensors()` - Initialize I2C and sensor
- `readI2CSensors()` - Read sensor values

**Features:**
- BME280 dual-address detection (0x76, 0x77)
- Temperature, pressure, humidity reading
- 2-second read throttling
- Automatic Celsius to Kelvin conversion

## Integration Guide

### Quick Start

1. **Include headers in your application:**
   ```cpp
   #include "hardware/nmea0183.h"
   #include "hardware/nmea2000.h"
   #include "hardware/sensors.h"
   ```

2. **Initialize in setup():**
   ```cpp
   void setup() {
       initNMEA2000();
       initI2CSensors();
   }
   ```

3. **Use in main loop:**
   ```cpp
   void loop() {
       NMEA2000.ParseMessages();
       readI2CSensors();
   }
   ```

4. **Handle NMEA 0183 data:**
   ```cpp
   void serialEvent1() {
       while (Serial1.available()) {
           String sentence = Serial1.readStringUntil('\n');
           parseNMEASentence(sentence);
       }
   }
   ```

### Required Global Variables

These must be declared in your main application:

```cpp
// GPS Data
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

// I2C Sensors
Adafruit_BME280 bme;
bool bmeEnabled = false;
unsigned long lastSensorRead = 0;

// CAN Bus
bool n2kEnabled = false;
```

### Required Helper Functions

These must be implemented in your main application:

```cpp
// Time
String iso8601Now();

// Conversions
double knotsToMS(double knots);
double degToRad(double deg);
double RadToDeg(double rad);
double KelvinToC(double kelvin);

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

## Pin Configuration

### NMEA 0183 (Serial1)
- RX: GPIO 21 (or GPIO 32 with GPIO header)
- TX: GPIO 22 (or GPIO 33 with GPIO header)
- Baud: 4800

### NMEA 2000 (CAN Bus)
- TX: GPIO 27
- RX: GPIO 26

### I2C Sensors
- SDA: GPIO 5
- SCL: GPIO 32
- Speed: 400 kHz

## Supported Data Types

### NMEA 0183 Sentence Types (15+)
- **RMC** - Recommended Minimum (position, speed, course)
- **GGA** - GPS Fix Data (position, satellites, altitude)
- **GLL** - Geographic Position (lat/lon)
- **VTG** - Track & Speed
- **HDG/HDM/HDT** - Heading (magnetic/true)
- **VHW** - Water Speed & Heading
- **DBT** - Depth Below Transducer
- **MWD** - Meteorological Composite (wind)
- **MWV** - Wind Speed & Angle
- **VDR** - Set & Drift (current)
- **VPW** - Speed Parallel to Wind
- **VWT** - Wind Speed & Angle True
- **WCV** - Waypoint Closure Velocity
- **XTE** - Cross-Track Error
- **ZDA** - Time & Date
- **GSV** - Satellites in View

### NMEA 2000 PGN Types (5)
- **129025** - Position (latitude, longitude)
- **129026** - COG & SOG
- **128267** - Water Depth
- **130306** - Wind Speed & Angle
- **130310** - Outside Environment (temperature, pressure)

### I2C Sensors
- **BME280** - Temperature, Pressure, Humidity

## Output Paths

All modules update SignalK paths via `setPathValue()`:

**NMEA 0183:**
- `navigation.position` - Combined position object
- `navigation.speedOverGround` - SOG in m/s
- `navigation.courseOverGroundTrue` - COG in radians
- `navigation.headingMagnetic` - Heading in radians
- `navigation.gnss.satellitesInView` - Satellite count
- `navigation.gnss.altitude` - Altitude in meters
- `environment.wind.*` - Wind data
- `environment.depth.belowTransducer` - Water depth
- `navigation.current.*` - Current data

**NMEA 2000:**
- Same paths as NMEA 0183 with source `nmea2000.can`

**Sensors:**
- `environment.inside.temperature` - Kelvin
- `environment.inside.pressure` - Pa
- `environment.inside.humidity` - 0-1 fraction

## Troubleshooting

### Data Not Appearing
1. Check serial/CAN connections
2. Verify baud rates and pin definitions
3. Monitor Serial output for error messages
4. Check `n2kEnabled`/`bmeEnabled` flags
5. Verify `setPathValue()` implementation

### Checksum Errors
- Normal warning messages (data still processed)
- Some devices don't send correct checksums
- Continue processing with warning

### Sensor Not Detected
- Try both I2C addresses (0x76 and 0x77)
- Check I2C clock speed
- Verify SDA/SCL connections
- Use I2C scanner to find device

## Reference Documentation

See the project root directory for:
- `HARDWARE_MODULES.md` - Detailed module documentation
- `HARDWARE_MODULES_QUICK_REFERENCE.md` - Quick reference and examples

## Code Extraction

Code was extracted from `main_original.cpp`:
- NMEA 0183: Lines 736-1117 (382 lines)
- NMEA 2000: Lines 3281-3408 (128 lines)
- Sensors: Lines 3411-3461 (51 lines)

Total: 803 lines of code in 6 files (3 headers, 3 implementations)

## License

Same as parent ESP32-SignalK project

---
**Version**: 1.0
**Status**: Ready for Integration
**Last Updated**: 2025-11-13
