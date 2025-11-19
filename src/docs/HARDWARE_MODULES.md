# ESP32-SignalK Hardware Modules

This document describes the refactored hardware modules extracted from `main_original.cpp`.

## Module Overview

### 1. NMEA 0183 Module (`hardware/nmea0183.*`)
**Purpose**: Parse NMEA 0183 serial sentences from GPS and marine instruments

**Header File**: `hardware/nmea0183.h` (70 lines)
**Implementation**: `hardware/nmea0183.cpp` (408 lines)

**Exported Functions**:
- `bool validateNmeaChecksum(const String& sentence)` - Validates NMEA checksum (XOR)
- `double nmeaCoordToDec(const String& coord, const String& hemisphere)` - Converts DDMM.MMMM to decimal degrees
- `std::vector<String> splitNMEA(const String& sentence)` - Splits sentence into comma-separated fields
- `void parseNMEASentence(const String& sentence)` - Main parser supporting 15+ sentence types

**Supported NMEA Sentence Types**:
- RMC: Recommended Minimum Position
- GGA: GPS Fix Data
- VTG: Track & Speed
- HDG/HDM/HDT: Heading (Magnetic/True)
- GLL: Geographic Position
- MWD/MWV: Wind Data
- VDR: Set and Drift (Current)
- VHW: Water Speed and Heading
- VPW: Speed Parallel to Wind
- VWT: Wind Speed and Angle True
- WCV: Waypoint Closure Velocity
- XTE: Cross-Track Error
- ZDA: Time & Date
- DBT: Depth of Water
- GSV: GPS Satellites in View

**External Dependencies**:
- `gpsData` - Global GPS data structure
- `iso8601Now()` - Timestamp function
- `setPathValue()` - Path value update functions
- `updateWindAlarm()`, `updateDepthAlarm()` - Alarm triggers
- `updateNavigationPosition()` - Navigation update

---

### 2. NMEA 2000 Module (`hardware/nmea2000.*`)
**Purpose**: Handle NMEA 2000 (CAN bus) marine data from N2K-capable instruments

**Header File**: `hardware/nmea2000.h` (61 lines)
**Implementation**: `hardware/nmea2000.cpp` (159 lines)

**Exported Functions**:
- `void HandleN2kPosition(const tN2kMsg &N2kMsg)` - PGN 129025: Lat/Lon position
- `void HandleN2kCOGSOG(const tN2kMsg &N2kMsg)` - PGN 129026: Course Over Ground, Speed Over Ground
- `void HandleN2kWindSpeed(const tN2kMsg &N2kMsg)` - PGN 130306: Wind speed and angle
- `void HandleN2kWaterDepth(const tN2kMsg &N2kMsg)` - PGN 128267: Water depth
- `void HandleN2kOutsideEnvironment(const tN2kMsg &N2kMsg)` - PGN 130310: Water/air temp, pressure
- `void initNMEA2000()` - Initialization and CAN bus setup

**Features**:
- NMEA2000 library integration
- Listen-only mode (no address claiming)
- Product and device information configuration
- Forward mode for debugging
- Proper error handling for N2K_NA values

**External Dependencies**:
- `gpsData` - Global GPS data structure
- `n2kEnabled` - Boolean flag for NMEA2000 availability
- `setPathValue()` - Path value update functions
- `updateWindAlarm()`, `updateDepthAlarm()` - Alarm triggers
- `RadToDeg()`, `KelvinToC()` - Unit conversion functions
- `NMEA2000` - External NMEA2000 library object

---

### 3. Sensors Module (`hardware/sensors.*`)
**Purpose**: Initialize and read from I2C environmental sensors

**Header File**: `hardware/sensors.h` (35 lines)
**Implementation**: `hardware/sensors.cpp` (70 lines)

**Exported Functions**:
- `void initI2CSensors()` - Initialize BME280 at 0x76 or 0x77
- `void readI2CSensors()` - Read and publish sensor values (throttled to 2-second intervals)

**Supported Sensors**:
- BME280: Temperature, Pressure, Humidity

**Sensor Data Updates**:
- `environment.inside.temperature` (Kelvin)
- `environment.inside.pressure` (Pa)
- `environment.inside.humidity` (0-1 relative humidity)

**Features**:
- I2C dual-address detection (0x76, 0x77)
- Configurable sampling (X1)
- Reading throttle to prevent excessive I2C traffic (2000ms)
- Automatic Celsius to Kelvin conversion

**External Dependencies**:
- `bme` - Global BME280 object
- `bmeEnabled` - Boolean flag for BME280 availability
- `lastSensorRead` - Timestamp for throttling
- `setPathValue()` - Path value update function

---

## Integration Notes

### File Locations
All hardware modules are located in:
```
c:\Users\akatsaris\projects\esp32-signalk\src\hardware\
```

### Including in Main Application
To use these modules in your main application, add these includes:

```cpp
#include "hardware/nmea0183.h"
#include "hardware/nmea2000.h"
#include "hardware/sensors.h"
```

And call initialization functions in setup():
```cpp
initNMEA2000();
initI2CSensors();
```

### Usage Pattern
```cpp
// In serial/CAN event handlers
parseNMEASentence(sentence);    // NMEA 0183 sentences

// In main loop
readI2CSensors();                 // Read I2C sensors
NMEA2000.ParseMessages();         // Process NMEA2000 messages
```

---

## Global Variables Required

The following global variables must be declared in main:

```cpp
struct GPSData {
  double lat, lon, sog, cog, heading, altitude;
  int satellites;
  String fixQuality, timestamp;
} gpsData;

Adafruit_BME280 bme;
bool bmeEnabled = false;
bool n2kEnabled = false;
unsigned long lastSensorRead = 0;
```

---

## Extracted Code Sections

### From main_original.cpp
- **NMEA 0183**: Lines 736-1117 (382 lines)
  - All parsing and validation functions
  - All sentence type handlers

- **NMEA 2000**: Lines 3281-3408 (128 lines)
  - All PGN message handlers
  - Initialization function

- **Sensors**: Lines 3411-3461 (51 lines)
  - BME280 initialization
  - Sensor reading with throttling

---

## Testing Recommendations

1. **NMEA 0183 Module**:
   - Test checksum validation with valid/invalid sentences
   - Test coordinate conversion (latitude/longitude)
   - Verify all 15+ sentence types parse correctly
   - Check for memory leaks with vector operations

2. **NMEA 2000 Module**:
   - Verify CAN bus initialization
   - Test message handlers with real N2K devices
   - Check for proper NA value handling
   - Monitor CAN bus traffic

3. **Sensors Module**:
   - Test BME280 initialization at both I2C addresses
   - Verify temperature/pressure/humidity readings
   - Check 2-second throttling timing
   - Test with sensor disconnected

---

**Total Lines of Code**: 803 lines
**Documentation**: This file

Generated for ESP32-SignalK Project Restructuring
