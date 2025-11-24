# LILYGO T-CAN485 Pinout Guide

## 📍 How to Find GPIO Pins on Your T-CAN485 Board

The GPIO numbers (like GPIO 32, GPIO 33) refer to the ESP32's internal GPIO numbers, **not physical pin positions**. You need to use the **12-pin GPIO header** on the board.

## 🔌 12-Pin GPIO Header Location

The T-CAN485 board has a **12-pin (2x6) header** that you need to solder. Look for the rectangular pad area on the board labeled with pin numbers.

```
┌─────────────────────────────────────┐
│   LILYGO TTGO T-CAN485 Board       │
│                                     │
│   [USB-C Port]                      │
│                                     │
│   [ESP32 Chip]                      │
│                                     │
│   ┌─────────────┐  ← 12-Pin Header │
│   │ ● ● ● ● ● ● │     (2 rows of 6)│
│   │ ● ● ● ● ● ● │     Solder pins  │
│   └─────────────┘     here!        │
│                                     │
│   [CAN/RS485 Terminals]             │
└─────────────────────────────────────┘
```

## 📊 12-Pin GPIO Header Pinout

Here's what the 12-pin header exposes (you need to check your specific board, but typically):

```
Pin Layout (Top View):
┌──────────────────────────────┐
│  1●  3●  5●  7●  9● 11●      │  Row 1
│                              │
│  2●  4●  6●  8● 10● 12●      │  Row 2
└──────────────────────────────┘

Typical Pin Assignments:
Pin 1:  3.3V
Pin 2:  GND
Pin 3:  GPIO 32  ← Seatalk1 RX (SoftwareSerial)
Pin 4:  GPIO 33  ← Single-Ended NMEA RX (UART2 with hardware inversion)
Pin 5:  GPIO 25  ← GPS RX (SoftwareSerial)
Pin 6:  GPIO 18  ← GPS TX (SoftwareSerial)
Pin 7:  GPIO 5   ← I2C SDA
Pin 8:  GPIO 34  ← I2C SCL (input-only)
Pin 9:  GPIO 35  ← Available
Pin 10: GPIO 36  ← Available (input-only)
Pin 11: GPIO 39  ← Available (input-only)
Pin 12: GND
```

**⚠️ Note**: The exact pin order may vary by board revision. Check the silkscreen on your board or measure with a multimeter!

## 🔧 Connection Guide

### For Single-Ended NMEA 0183 (Direct Connection with Optocoupler)

**⭐ NEW: Hardware inverted RX support for optocouplers!**

```
NMEA Device          T-CAN485 GPIO Header
─────────────────────────────────────────
NMEA OUT      ────→  Optocoupler ────→ GPIO 33 (Pin 4) - RX
GND           ────→  GND (Pin 2 or 12)

Note: Hardware RX inversion enabled automatically
      Plug-and-play with optocouplers (PC817, etc.)
      No external inverter circuit needed!
```

**📖 See [docs/SINGLE_ENDED_NMEA.md](../../docs/SINGLE_ENDED_NMEA.md) for complete wiring guide**

### For GPS Module (SoftwareSerial)

```
GPS Module           T-CAN485 GPIO Header
─────────────────────────────────────────
TX (transmit) ────→  GPIO 25 (Pin 5) - RX (SoftwareSerial)
RX (receive)  ────→  GPIO 18 (Pin 6) - TX (SoftwareSerial)
VCC           ────→  3.3V (Pin 1)
GND           ────→  GND (Pin 2 or 12)

Note: GPS moved to SoftwareSerial to free UART2 for Single-Ended NMEA
```

### For Seatalk1 (SoftwareSerial)

```
Seatalk Level Shifter   T-CAN485 GPIO Header
─────────────────────────────────────────────
Output (3.3V)    ────→  GPIO 32 (Pin 3) - RX
GND              ────→  GND (Pin 2 or 12)

Note: Uses SoftwareSerial - no conflict with RS485 or GPS!
      GPIO 32 is now dedicated to Seatalk1
```

### For BME280 Sensor (I2C)

```
BME280 Sensor        T-CAN485 GPIO Header
─────────────────────────────────────────
SDA           ────→  GPIO 5 (Pin 7)
SCL           ────→  GPIO 34 (Pin 8) - Input-only
VCC           ────→  3.3V (Pin 1)
GND           ────→  GND (Pin 2 or 12)
```

## 🚫 DO NOT USE These Pins (Already Used by Hardware)

| GPIO | Function | Why You Can't Use It |
|------|----------|---------------------|
| GPIO 2 | SD MISO | SD Card interface |
| GPIO 13 | SD CS | SD Card interface |
| GPIO 14 | SD SCLK | SD Card interface |
| GPIO 15 | SD MOSI | SD Card interface |
| GPIO 4 | WS2812 LED | RGB LED control |
| GPIO 16 | Power Enable | ME2107 booster chip |
| GPIO 17 | RS485 DE | RS485 control |
| GPIO 19 | RS485 Enable | RS485 control |
| GPIO 21 | RS485 RX | RS485 receive |
| GPIO 22 | RS485 TX | RS485 transmit |
| GPIO 23 | CAN SE | CAN transceiver |
| GPIO 26 | CAN RX | CAN receive |
| GPIO 27 | CAN TX | CAN transmit |

## 🟢 GPIO Pin Usage Summary

| GPIO | Status | Usage | Notes |
|------|--------|-------|-------|
| GPIO 5 | ✅ Available | I2C SDA | Can be used for BME280 |
| GPIO 18 | ✅ Used | GPS TX (SoftwareSerial) | Fixed assignment |
| GPIO 25 | ✅ Used | GPS RX (SoftwareSerial) | Fixed assignment |
| GPIO 32 | ✅ Used | Seatalk1 RX (SoftwareSerial) | No conflicts! |
| GPIO 33 | ✅ Used | Single-Ended NMEA RX (UART2) | Hardware inverted RX |
| GPIO 34 | ✅ Available | I2C SCL | Input-only, pull-up required |
| GPIO 35 | 🟡 Available | ADC / Input | Input-only |
| GPIO 36 | 🟡 Available | ADC / Input | Input-only |
| GPIO 39 | 🟡 Available | ADC / Input | Input-only |

## 🔍 How to Identify Pins on Your Physical Board

### Method 1: Check Silkscreen
Look at the white text printed on the PCB near the GPIO header. It should show pin numbers or GPIO numbers.

### Method 2: Use a Multimeter
1. Set multimeter to continuity mode
2. Touch one probe to a known pin (like 3.3V or GND)
3. Touch the other probe to header pins until you find a match
4. Use the schematic to trace other pins

### Method 3: Check Official Documentation
1. Visit: https://github.com/Xinyuan-LilyGO/T-CAN485
2. Download the schematic PDF from the `/project` folder
3. Look for the GPIO header section in the schematic

## 📐 Physical Pin Measurements

To help identify pins without documentation:

1. **Power Pins (3.3V/GND)**:
   - Measure with multimeter
   - 3.3V should read ~3.3V
   - GND should read 0V or continuity with USB ground

2. **GPIO Pins**:
   - Most will read 3.3V when not connected (pulled high internally)
   - Some may read 0V (pulled low or floating)

## 🔌 Recommended Wire Connections

### Dupont Connectors
- Use standard 2.54mm (0.1") female Dupont connectors
- Solder a male header to the GPIO header first
- Then connect using Dupont wires

### Direct Soldering
- Solder wires directly to the pads if you don't want to use headers
- Use different colored wires for easy identification:
  - **Red**: 3.3V
  - **Black**: GND
  - **Yellow**: RX pins
  - **Green**: TX pins
  - **Blue**: SDA
  - **White**: SCL

## 📸 Board Photos

Look for these landmarks on your board to orient yourself:

```
[USB-C Port Side]
     ↓
┌─────────────────┐
│   T-CAN485      │
│                 │
│  [ESP32 Chip]   │  ← Large square chip in center
│                 │
│  [GPIO Header]  │  ← Look for 12 holes/pads
│  (12 pins)      │     Usually near edge
│                 │
│  [CAN/RS485]    │  ← Green terminal blocks
│  [Terminals]    │
└─────────────────┘
[Terminal Block Side]
```

## ⚡ Power Requirements

- **Input**: 5-12V DC via terminal block OR 5V via USB-C
- **GPIO Output**: 3.3V logic level (NOT 5V tolerant!)
- **Max Current per GPIO**: 12mA (use external buffers for high current)

## 🛡️ Safety Notes

1. **Never connect 5V devices directly to 3.3V GPIOs** - Use level shifters
2. **Don't exceed 12mA per GPIO** - ESP32 can be damaged
3. **Use proper CAN termination** - 120Ω resistor required at each end of CAN bus
4. **Ground all devices together** - Connect all GND pins to common ground

## 🧪 Testing Your Connections

After connecting, upload the firmware and check the serial monitor:

```
Starting NMEA UART...
NMEA0183 via RS485 started on terminal blocks A/B

Starting GPS module...
GPS SoftwareSerial started on pins RX:25 TX:18 @ 9600 baud

=== Single-Ended NMEA 0183 Input ===
Single-Ended NMEA initialized on GPIO 33 @ 4800 baud (UART2)
Mode: Hardware RX inversion enabled for optocoupler compatibility

=== Seatalk 1 Initialization ===
Seatalk 1 initialized successfully

Initializing NMEA2000...
NMEA2000 initialized successfully

Initializing I2C sensors...
BME280 sensor found!
```

If you see these messages, your connections are correct!

## 📞 Need Help?

1. **Check the serial monitor** - It will tell you if hardware is detected
2. **Use a multimeter** - Verify pin voltages and connections
3. **Check the official repo** - https://github.com/Xinyuan-LilyGO/T-CAN485
4. **Test one connection at a time** - Don't connect everything at once

---

**Last Updated**: 2025
**Board**: LILYGO TTGO T-CAN485
**Firmware**: ESP32 SignalK Gateway
