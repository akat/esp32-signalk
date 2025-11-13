# RS485 Connection Guide for T-CAN485

## 🔌 Understanding RS485 vs Standard UART

Your T-CAN485 board has a **built-in RS485 transceiver** that allows you to connect NMEA 0183 devices that use RS485 differential signaling.

### What's the Difference?

| Feature | Standard UART (TTL) | RS485 |
|---------|---------------------|-------|
| Signal Type | Single-ended (0V/3.3V) | Differential (A/B) |
| Max Distance | ~15 meters | **1200 meters** |
| Noise Immunity | Low | **Very High** |
| Multi-device | No (point-to-point) | **Yes (bus topology)** |
| Common Use | GPS modules, sensors | Marine instruments, industrial |

### Which Devices Use RS485?

**RS485 Devices (Use Terminal Blocks):**
- Modern marine instruments (Raymarine, Garmin, B&G)
- Networked depth sounders
- Wind instruments
- Autopilots
- Multi-function displays (MFDs)
- AIS transponders

**TTL UART Devices (Use GPIO Header):**
- Most GPS modules (NEO-6M, NEO-M8N)
- Simple sensors
- Older NMEA devices
- Direct microcontroller connections

## 🎛️ T-CAN485 Terminal Blocks

Your board has **two green terminal blocks**:

```
┌─────────────────────────────┐
│  LILYGO T-CAN485 Board      │
│                             │
│  [USB-C]    [ESP32]         │
│                             │
│  ┌───────┐   ┌───────┐     │
│  │ CAN   │   │ RS485 │     │  ← Green Terminal Blocks
│  │ H   L │   │ A   B │     │
│  └───────┘   └───────┘     │
│                             │
└─────────────────────────────┘

CAN Terminal (Left):
├─ H (CANH) → NMEA 2000 High
└─ L (CANL) → NMEA 2000 Low

RS485 Terminal (Right):
├─ A (RS485+) → NMEA 0183 A/Data+
└─ B (RS485-) → NMEA 0183 B/Data-
```

## ⚙️ Configuration

### Step 1: Choose Your Connection Method

Open `src/main.cpp` and find this line (around line 57):

```cpp
#define USE_RS485_FOR_NMEA    // ← Comment this line to use GPIO header instead
```

**To use RS485 terminal blocks (recommended for marine instruments):**
- Leave the line uncommented (as shown above)

**To use GPIO header pins (for GPS modules):**
- Comment out the line:
```cpp
// #define USE_RS485_FOR_NMEA    // ← Commented = GPIO header mode
```

### Step 2: Connect Your Device

#### Option A: Using RS485 Terminal Blocks

```
Marine Instrument       T-CAN485 RS485 Terminal
─────────────────────────────────────────────────
A or Data+ or TX+  ───→  A (RS485+)
B or Data- or TX-  ───→  B (RS485-)
GND                ───→  GND (via power terminal)
12V (if needed)    ───→  12V input terminal
```

**Important Notes:**
- **Polarity matters!** A goes to A, B goes to B
- Some devices label as: Data+/Data-, TX+/TX-, or simply A/B
- RS485 is differential - both wires carry the signal
- You can connect **multiple devices** on the same bus (up to 32 devices)

#### Option B: Using GPIO Header Pins

```
GPS/Sensor Device   T-CAN485 GPIO Header
────────────────────────────────────────
TX (transmit)  ───→  GPIO 32 (Pin 3) - RX
RX (receive)   ───→  GPIO 33 (Pin 4) - TX
VCC            ───→  3.3V (Pin 1)
GND            ───→  GND (Pin 2)
```

### Step 3: Build and Upload

```bash
# Build the firmware
pio run

# Upload to board
pio run --target upload

# Monitor serial output
pio device monitor
```

### Step 4: Verify Connection

Check the serial monitor output:

**If using RS485:**
```
Starting NMEA UART...
NMEA0183 via RS485 started on terminal blocks A/B
Using built-in RS485 transceiver (GPIO 21/22)
```

**If using GPIO header:**
```
Starting NMEA UART...
NMEA0183 UART started on GPIO header pins RX:32 TX:33
```

## 🔧 Wiring Examples

### Example 1: Single RS485 Instrument

```
┌────────────────────┐
│  Depth Sounder     │
│  (RS485 Output)    │
│                    │
│  A+ ●──────────────┼──→ A (T-CAN485)
│  B- ●──────────────┼──→ B (T-CAN485)
│  GND ●─────────────┼──→ GND
└────────────────────┘
```

### Example 2: Multiple RS485 Devices (Bus)

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Wind       │    │  Depth      │    │  T-CAN485   │
│  Sensor     │    │  Sounder    │    │  Board      │
│             │    │             │    │             │
│  A ●────────┼────┼─● A  ● A───┼────┼─● A         │
│  B ●────────┼────┼─● B  ● B───┼────┼─● B         │
│             │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘
     120Ω                                  120Ω
  Terminator                            Terminator
  (Optional)                           (Required)
```

**Important for Multiple Devices:**
- Connect all A wires together
- Connect all B wires together
- Use twisted pair cable
- Add 120Ω termination resistor at each end of the bus
- Keep total cable length under 1200m

### Example 3: Mixed Setup (RS485 + GPS)

You can use **both** RS485 and GPIO header simultaneously:

```
RS485 Marine Instruments → T-CAN485 Terminal Blocks (A/B)
                            ↓
                         Serial1 (GPIO 21/22)
                            ↓
                     ESP32 Processing
                            ↓
                         Serial2 (GPIO 25/18)
                            ↓
GPS Module (TTL UART) → T-CAN485 GPIO Header (Pins 5/6)
```

## 📊 RS485 Signal Characteristics

### Voltage Levels
- **Differential**: 1.5V to 5V between A and B
- **Common Mode**: -7V to +12V
- **Logic 1 (Mark)**: A > B (A higher than B)
- **Logic 0 (Space)**: B > A (B higher than A)

### Data Format (NMEA 0183)
- **Baud Rate**: 4800 bps (default) or 38400 bps (high-speed)
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Format**: 8N1

## 🐛 Troubleshooting

### Problem: No Data Received

**Check These:**
1. **Verify Wiring**:
   - A to A, B to B (not crossed!)
   - Use multimeter to check continuity

2. **Check Power**:
   - Is the marine instrument powered on?
   - Does it show NMEA output on its display?

3. **Verify Configuration**:
   - Is `#define USE_RS485_FOR_NMEA` uncommented?
   - Did you rebuild and upload after changing?

4. **Serial Monitor**:
   ```bash
   pio device monitor --baud 115200
   ```
   Look for: `NMEA0183 via RS485 started...`

5. **Test with Multimeter**:
   - Measure voltage between A and B
   - Should see ~0V idle, fluctuating when data transmitted
   - Both A and B should be ~2.5V relative to ground

### Problem: Garbled Data

**Possible Causes:**
1. **Wrong Baud Rate**:
   - Change `NMEA_BAUD` to 38400 if needed
   - Rebuild and upload

2. **Crossed Wires**:
   - Swap A and B connections
   - Try both ways if not labeled clearly

3. **Cable Too Long**:
   - RS485 max: 1200m at 4800 baud
   - Use shielded twisted pair
   - Add termination resistors

4. **Ground Loops**:
   - Connect all device grounds together
   - Use isolated RS485 if problems persist

### Problem: Intermittent Connection

**Solutions:**
1. **Add Termination**:
   - 120Ω resistor between A and B at each end
   - Critical for long cables or multiple devices

2. **Check Cable Quality**:
   - Use twisted pair cable
   - Shield should be grounded at one end only
   - Avoid running parallel to power cables

3. **Bias Resistors**:
   - Some setups need pull-up/pull-down resistors
   - T-CAN485 has built-in biasing (usually sufficient)

## 📏 Cable Specifications

### Recommended Cable
- **Type**: Shielded twisted pair (STP)
- **Impedance**: 120Ω characteristic impedance
- **Wire Gauge**: 22-26 AWG
- **Examples**:
  - Belden 3105A (RS485 cable)
  - Alpha Wire 6162
  - Any Category 5e/6 ethernet cable (use one pair)

### Maximum Lengths
| Baud Rate | Max Distance |
|-----------|--------------|
| 4800 bps  | 1200 meters |
| 9600 bps  | 1000 meters |
| 38400 bps | 500 meters |
| 115200 bps| 150 meters |

## 🔍 Testing Your RS485 Connection

### Test 1: Loopback Test

Connect A to A and B to B directly (short wire):

```
T-CAN485     Test Wire     T-CAN485
RS485 Terminal            GPIO Header
─────────────────────────────────────
A (terminal)  ───→  GPIO 33 (TX)
B (terminal)  ───→  GPIO 32 (RX)
```

If this works, your RS485 transceiver is functional.

### Test 2: Voltage Measurement

With instrument connected and powered:

```bash
# Use multimeter in DC voltage mode
# Black probe to GND
# Red probe to test point

Measure A-GND: Should be ~2.5V ± 0.5V
Measure B-GND: Should be ~2.5V ± 0.5V
Measure A-B:   Should fluctuate ±1-3V when transmitting
```

### Test 3: Serial Monitor

Watch for NMEA sentences:

```
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
```

If you see these, RS485 is working!

## 📘 Advanced Configuration

### Changing Baud Rate

Edit `main.cpp`:

```cpp
#ifdef USE_RS485_FOR_NMEA
  #define NMEA_BAUD 38400  // Change from 4800 to 38400
#endif
```

Common baud rates:
- 4800: Standard NMEA 0183
- 9600: Some GPS units
- 38400: High-speed NMEA (AIS, radar)

### Adding Termination Resistor

Connect 120Ω resistor between A and B:
- At the far end of the cable
- At the T-CAN485 end (if it's an endpoint)

**Note**: T-CAN485 doesn't have built-in termination - you must add external resistor if needed.

## 🛡️ Safety Notes

1. **Never Hot-Plug RS485**:
   - Power off devices before connecting/disconnecting
   - RS485 transceivers can be damaged by ESD

2. **Grounding**:
   - Always connect ground between devices
   - Shield ground at one end only (to avoid ground loops)

3. **Voltage Protection**:
   - RS485 can handle ±7V common mode
   - Use surge protectors in lightning-prone areas

4. **Cable Routing**:
   - Keep away from AC power lines
   - Avoid sharp bends in cable
   - Secure with cable ties

## 📞 Getting Help

If you're still having trouble:

1. **Check Serial Monitor** - Most issues show up here
2. **Use Multimeter** - Verify voltages and continuity
3. **Try Loopback Test** - Isolate hardware vs wiring issues
4. **Swap Cables** - Rule out bad cables
5. **Check Instrument Manual** - Verify NMEA output is enabled

---

**Last Updated**: 2025
**Board**: LILYGO TTGO T-CAN485
**Firmware**: ESP32 SignalK Gateway
