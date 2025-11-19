# Quick Seatalk1 Connection Guide

## ✅ You Have Everything Ready!

Your firmware is **compiled and ready** with Seatalk1 enabled on GPIO 32 (SoftwareSerial).

## 🔌 Hardware Connection

### Level Shifter to ESP32

```
12V Level Shifter Output    →    ESP32 T-CAN485
─────────────────────────────────────────────────
3.3V Output (TTL)            →    GPIO 32 (Pin 3 on GPIO header)
GND                          →    GND (Pin 2 or Pin 12)
```

### Seatalk Bus to Level Shifter

```
Seatalk Bus                  →    12V Level Shifter
─────────────────────────────────────────────────
Yellow Wire (Data)           →    Input (12V side)
Black/Shield (Ground)        →    GND
Red Wire (+12V)              →    ⚠️ DO NOT CONNECT to ESP32!
```

## 📍 GPIO Header Pin Location

The T-CAN485 board has a **12-pin (2x6) header**:

```
Pin Layout (Top View):
┌──────────────────────────────┐
│  1●  3●  5●  7●  9● 11●      │  Row 1
│                              │
│  2●  4●  6●  8● 10● 12●      │  Row 2
└──────────────────────────────┘

Pin Assignments:
Pin 1:  3.3V        ← Power for GPS/sensors
Pin 2:  GND         ← Ground (use for Seatalk shifter)
Pin 3:  GPIO 32     ← 🟡 SEATALK1 RX (connect level shifter here!)
Pin 4:  GPIO 33     ← Available
Pin 5:  GPIO 25     ← GPS RX (if using GPS module)
Pin 6:  GPIO 18     ← GPS TX
Pin 7:  GPIO 5      ← I2C SDA
Pin 8:  GPIO 34     ← I2C SCL
Pin 9:  GPIO 35     ← Available
Pin 10: GPIO 36     ← Available
Pin 11: GPIO 39     ← Available
Pin 12: GND         ← Ground (alternative)
```

## ⚡ Wiring Steps

### Step 1: Connect Level Shifter Output to ESP32

1. **GPIO 32 (Pin 3)**: Connect level shifter 3.3V output
2. **GND (Pin 2)**: Connect level shifter ground

### Step 2: Connect Seatalk Bus to Level Shifter

1. **Yellow wire**: Seatalk data → Level shifter input (12V side)
2. **Black/Shield**: Ground → Level shifter ground
3. **Red wire**: **DO NOT CONNECT** (this is 12V power - will damage ESP32!)

## 🎨 Color-Coded Wiring (Recommended)

Use colored wires to avoid confusion:

```
Color         Connection
──────────────────────────────────────────
Yellow/Green  → GPIO 32 (Seatalk data)
Black         → GND (Common ground)
Red           → ⚠️ DO NOT CONNECT!
```

## 🔍 Visual Check Before Power-On

Before uploading firmware and powering on:

✅ Level shifter output (3.3V TTL) connected to GPIO 32 (Pin 3)
✅ Ground connected between shifter and ESP32
✅ Seatalk yellow wire connected to level shifter input
✅ Seatalk ground connected to level shifter ground
❌ **NO direct 12V connection to ESP32**
❌ **Seatalk red wire NOT connected to ESP32**

## 📤 Upload Firmware

```bash
platformio run --target upload
platformio device monitor --baud 115200
```

## 📺 Expected Serial Output

When you power on, you should see:

```
=== ESP32 SignalK Server ===

Starting GPS module...
GPS UART started on pins RX:25 TX:18

=== Seatalk 1 Initialization ===
Mode: SoftwareSerial (no RS485/GPS conflicts)
SoftwareSerial initialized successfully
Seatalk RX Pin: GPIO 32
Baud Rate: 4800
Signal: Inverted (12V bus via level shifter)

*** HARDWARE REQUIREMENTS ***
1. Opto-isolated level shifter (12V → 3.3V)
2. Inverted logic (handled by SoftwareSerial)
3. Seatalk wiring:
   - Yellow wire → Level shifter input
   - Red wire → +12V (keep isolated!)
   - Black/Shield → Ground (common with ESP32)

*** WARNING ***
Never connect Seatalk directly to ESP32!
12V will damage the GPIO pins.
Always use proper level shifting/isolation.
================================

Seatalk 1 initialized successfully

NMEA2000 initialized successfully
```

## 📊 Testing Seatalk Data

When Seatalk instruments are powered and transmitting, you'll see:

```
Seatalk [0x00]: 02 YZ XX XX
Depth: 5.23 m (17.2 ft)

Seatalk [0x10]: 01 XX YY
Apparent Wind Angle: 45.0° (Starboard)

Seatalk [0x11]: 01 XX 0Y
Apparent Wind Speed: 12.5 kn (6.43 m/s)

Seatalk [0x20]: 01 XX XX
Speed Through Water: 5.2 kn (2.67 m/s)
```

## 🔧 Troubleshooting

### No Seatalk Data

1. **Check wiring:**
   - Level shifter output → GPIO 32 (Pin 3)
   - Ground connected properly
   - Seatalk yellow wire → Level shifter input

2. **Check Seatalk instruments:**
   - Are they powered on?
   - Are they transmitting? (check with oscilloscope if possible)
   - Is the Seatalk bus connected properly?

3. **Check level shifter:**
   - Is it powered? (some need 3.3V or 5V)
   - Is the output 3.3V TTL? (not 5V!)
   - Is the signal inverted? (Seatalk needs inverted logic)

4. **Enable debug output:**
   Already enabled by default! Check serial monitor for any error messages.

### Garbled Data

- **Check signal inversion:** Seatalk uses inverted logic (idle = LOW)
- **Check baud rate:** Should be 4800 baud (already configured)
- **Check level shifter:** Some optocouplers invert, some don't

### ESP32 Rebooting / Crashing

- **⚠️ STOP IMMEDIATELY!** You might have 12V on GPIO 32!
- Disconnect everything and check wiring
- **NEVER** connect Seatalk directly - always use level shifter
- Verify level shifter output is 3.3V, not 12V

## 🎯 What Seatalk Data You'll Get

Once working, Seatalk1 will provide:

| Seatalk Command | SignalK Path | Data |
|-----------------|--------------|------|
| 0x00 | `environment.depth.belowTransducer` | Depth below transducer |
| 0x10 | `environment.wind.angleApparent` | Apparent wind angle |
| 0x11 | `environment.wind.speedApparent` | Apparent wind speed |
| 0x20 | `navigation.speedThroughWater` | Speed through water |
| 0x23 | `environment.water.temperature` | Water temperature |
| 0x9C | `navigation.headingMagnetic` | Magnetic heading |
| 0x84 | `steering.autopilot.target.headingMagnetic` | Autopilot course |

## 📱 View in SignalK Dashboard

Open browser: `http://192.168.4.1:3000`

You'll see all Seatalk data integrated with GPS, NMEA2000, and RS485 data!

## ✅ Success Criteria

You'll know it's working when:

1. ✅ Serial monitor shows "Seatalk 1 initialized successfully"
2. ✅ You see Seatalk command messages like `[0x00]`, `[0x10]`, etc.
3. ✅ Decoded data appears: "Depth: X.XX m", "Wind: XX kn"
4. ✅ SignalK dashboard shows Seatalk data updating in real-time
5. ✅ No ESP32 reboots or crashes

## 🔐 Safety Reminders

- ⚠️ **NEVER** connect 12V directly to ESP32 GPIO
- ⚠️ **ALWAYS** use a proper level shifter
- ⚠️ **CHECK** output voltage with multimeter (should be 3.3V)
- ⚠️ **TEST** level shifter separately before connecting to ESP32

## 📞 Need Help?

1. Check serial monitor output for error messages
2. Verify wiring with multimeter (continuity test)
3. Test level shifter output voltage (should be 3.3V)
4. Take photo of your wiring and check against diagram

---

**Good luck! Your Raymarine instruments will talk to your ESP32!** 🚢⚓

---

**Last Updated:** January 2025
**Firmware:** ESP32-SignalK with SoftwareSerial Seatalk1
**GPIO:** GPIO 32 (Pin 3 on GPIO header)
