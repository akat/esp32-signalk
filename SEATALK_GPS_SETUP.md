# Seatalk1 + GPS Setup Guide

## Overview

This firmware now supports **simultaneous Seatalk1 and GPS** with **ZERO conflicts**!

```
┌────────────────────────────────────────────────┐
│  ESP32 TTGO T-CAN485 - Full Configuration     │
├────────────────────────────────────────────────┤
│                                                │
│  🔴 NMEA2000 CAN Bus (Hardware CAN)            │
│     ├─ GPIO 26: CAN RX                         │
│     ├─ GPIO 27: CAN TX                         │
│     └─ GPIO 23: CAN SE (enable)                │
│                                                │
│  🟢 RS485 Depth Sounder (Serial1)              │
│     ├─ GPIO 21: RS485 RX                       │
│     ├─ GPIO 22: RS485 TX                       │
│     ├─ GPIO 17: DE control                     │
│     └─ GPIO 19: DE_ENABLE                      │
│                                                │
│  🟢 Single-Ended NMEA (UART2) ⭐ NEW!          │
│     ├─ GPIO 33: Single-Ended NMEA RX           │
│     └─ Hardware inverted for optocouplers      │
│                                                │
│  🟢 GPS Module (SoftwareSerial)                │
│     ├─ GPIO 25: GPS RX                         │
│     └─ GPIO 18: GPS TX                         │
│                                                │
│  🟡 Seatalk1 (SoftwareSerial)                  │
│     └─ GPIO 32: Seatalk RX (inverted)          │
│                                                │
│  🔵 I2C Sensors (Optional)                     │
│     ├─ GPIO 5: SDA                             │
│     └─ GPIO 34: SCL                            │
│                                                │
│  💡 RGB LED                                    │
│     └─ GPIO 4: WS2812                          │
│                                                │
└────────────────────────────────────────────────┘
```

## What Changed?

### ✅ Universal NMEA Support - All Peripherals Simultaneously

**Current Configuration:**
- **RS485** uses **UART1 (Serial1)** → Depth sounders, AIS, etc.
- **Single-Ended NMEA** uses **UART2 (HardwareSerial)** → Wind instruments with optocoupler support
- **GPS** uses **SoftwareSerial** → Position data
- **Seatalk1** uses **SoftwareSerial on GPIO 32** → Raymarine instruments

**✅ Result:** ALL peripherals work simultaneously with ZERO conflicts!

## Enabling Seatalk1

### Step 1: Uncomment in config.h

Open `src/config.h` and uncomment this line:

```cpp
#define USE_SEATALK1              // Uncomment to enable Seatalk 1
```

### Step 2: Hardware Connection

Connect your Seatalk1 level shifter to GPIO 32:

```
Seatalk Bus          Level Shifter        ESP32 T-CAN485
────────────────────────────────────────────────────────
Yellow (Data) ─────→ Input (12V side) ──→ (converts to 3.3V)
                     Output (3.3V)    ──→ GPIO 32 (Pin 3*)
Ground ────────────→ GND ─────────────────→ GND
Red (+12V) ────────→ DO NOT CONNECT!

* See GPIO header pinout in PINOUT.md
```

### Step 3: Build & Upload

```bash
platformio run --target upload
```

## GPS Module Connection

### NEO-6M or UBX-M10050

Both modules work identically:

```
GPS Module           ESP32 T-CAN485
────────────────────────────────────
VCC (3.3V-5V) ────→  3.3V (Pin 1)
GND           ────→  GND  (Pin 2)
TX            ────→  GPIO 25 (Pin 5)
RX            ────→  GPIO 18 (Pin 6) [Optional]
```

**Baud Rate:** 9600 (already configured in config.h)

## Supported GPS Modules

| Module | Chipset | Price | Performance | Recommendation |
|--------|---------|-------|-------------|----------------|
| **NEO-6M** | u-blox 6 | €4-6 | GPS only | ✅ Budget/Testing |
| **NEO-M8N** | u-blox 8 | €8-12 | GPS+GLONASS | ✅ Good balance |
| **UBX-M10050** | u-blox 10 | €10-15 | Multi-GNSS | ✅ Best (recommended) |

### Why UBX-M10050?
- ✅ Multi-GNSS (GPS + Galileo + GLONASS + BeiDou)
- ✅ Better sensitivity (-167 dBm vs -161 dBm)
- ✅ Faster fix (~24s vs ~27s)
- ✅ Lower power (25mA vs 45mA)
- ✅ 18 Hz update rate (vs 5 Hz)

## Expected Serial Output

When both GPS and Seatalk1 are enabled:

```
Starting GPS module...
GPS UART started on pins RX:25 TX:18

=== Seatalk 1 Initialization ===
Mode: SoftwareSerial (no RS485/GPS conflicts)
SoftwareSerial initialized successfully
Seatalk RX Pin: GPIO 32
Baud Rate: 4800
Signal: Inverted (12V bus via level shifter)
Seatalk 1 initialized successfully
================================

NMEA [GPS]: $GPGGA,123456.00,3745.1234,N,02345.6789,E,1,08,1.2,123.4,M,34.2,M,,*5E
NMEA [GPS]: $GPRMC,123456.00,A,3745.1234,N,02345.6789,E,0.5,180.2,010125,,,A*7F

Seatalk [0x00]: Depth Below Transducer
Depth: 5.23 m (17.2 ft)

Seatalk [0x10]: Apparent Wind Angle
Apparent Wind Angle: 45.0° (Starboard)

Seatalk [0x11]: Apparent Wind Speed
Apparent Wind Speed: 12.5 kn (6.43 m/s)
```

## SignalK Data Paths

### From GPS (NMEA 0183)
- `navigation.position` - Latitude/Longitude
- `navigation.speedOverGround` - SOG
- `navigation.courseOverGround` - COG
- `navigation.datetime` - GPS time
- `navigation.gnss.satellitesInView` - Satellite count

### From Seatalk1
- `environment.depth.belowTransducer` - Depth
- `environment.wind.angleApparent` - Apparent wind angle
- `environment.wind.speedApparent` - Apparent wind speed
- `navigation.speedThroughWater` - Speed through water
- `environment.water.temperature` - Water temperature
- `navigation.headingMagnetic` - Magnetic heading

### From RS485 Depth Sounder
- `environment.depth.belowTransducer` - Depth (NMEA DBT/DPT)

### From NMEA2000 CAN
- All standard NMEA2000 PGNs (Position, Wind, Depth, etc.)

## Performance Notes

### SoftwareSerial Limitations
- ✅ **Works perfectly for Seatalk1** (4800 baud, RX-only)
- ✅ No CPU overhead concerns at low baud rates
- ✅ Inverted logic supported natively
- ⚠️ Not suitable for high-speed bidirectional communication
- ⚠️ May have issues above 9600 baud (not a problem for Seatalk)

### Why It Works
1. **Seatalk1 is RX-only** - No TX needed (one-wire bus)
2. **Low baud rate** - 4800 baud is easy for SoftwareSerial
3. **Inverted logic** - EspSoftwareSerial handles it natively
4. **No timing conflicts** - GPS (Serial2) and RS485 (Serial1) use hardware UART

## Troubleshooting

### GPS Not Working
1. Check wiring (TX → GPIO 25)
2. Verify 3.3V power
3. Wait 1-2 minutes for first fix (cold start)
4. Check antenna has clear sky view
5. Monitor serial output for NMEA sentences

### Seatalk1 Not Working
1. Verify `USE_SEATALK1` is uncommented in config.h
2. Check level shifter wiring (12V → 3.3V)
3. Verify GPIO 32 connection
4. Enable debug: `setSeatalk1Debug(true)`
5. Check Seatalk yellow wire has data (oscilloscope)

### RS485 Depth Sounder Not Working
1. Check baud rate (4800 or 9600) in config.h
2. Try swapping A/B wires (reversed polarity)
3. Verify DE/RE pins are configured correctly
4. Check depth sounder power

### CAN Bus Not Working
1. Verify 120Ω termination resistors
2. Check CAN_H/CAN_L wiring
3. Ensure SE pin is LOW (enabled)
4. Monitor CAN traffic with external tool

## Hardware Shopping List

### Core Components
- ✅ **ESP32 TTGO T-CAN485** - €20-25 (AliExpress)
- ✅ **GPS Module** (NEO-6M or UBX-M10050) - €4-15
- ✅ **Seatalk1 Level Shifter** - PC817 optocoupler + resistors (~€2)
- ✅ **RS485 Depth Sounder** - Compatible with built-in transceiver

### Optional
- **BME280 Sensor** - Temperature/Pressure/Humidity (€3-5)
- **External GPS Antenna** - SMA connector (€5-10)
- **CAN Terminators** - 120Ω resistors (€1)

## Further Reading

- [PINOUT.md](src/docs/PINOUT.md) - Detailed GPIO mapping
- [SEATALK1_QUICKSTART.md](docs/SEATALK1_QUICKSTART.md) - Seatalk hardware guide
- [SEATALK1_HARDWARE.md](docs/SEATALK1_HARDWARE.md) - Level shifter circuits
- [RS485-GUIDE.md](src/docs/RS485-GUIDE.md) - Depth sounder setup
- [NMEA2000-GUIDE.md](src/docs/NMEA2000-GUIDE.md) - CAN bus setup

## Summary

✅ **All 4 input sources work simultaneously:**
1. **GPS** (Serial2 / GPIO 25/18)
2. **RS485 Depth** (Serial1 / GPIO 21/22)
3. **Seatalk1** (SoftwareSerial / GPIO 32)
4. **NMEA2000** (CAN / GPIO 26/27)

No conflicts. No compromises. Just works! 🚀

---

**Last Updated:** January 2025
**Firmware Version:** ESP32-SignalK v2.0+
**Board:** LILYGO TTGO T-CAN485
