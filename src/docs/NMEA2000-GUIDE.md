# NMEA 2000 Connection Guide for T-CAN485

## 🚢 Understanding NMEA 2000

NMEA 2000 (also called N2K) is a modern marine network standard that uses **CAN bus** technology. Unlike NMEA 0183 (serial), NMEA 2000 allows multiple devices to communicate on a single network backbone.

### What's the Difference?

| Feature | NMEA 0183 | NMEA 2000 |
|---------|-----------|-----------|
| Topology | Point-to-point | Multi-drop network bus |
| Wiring | 2-wire (TX/RX) or RS485 | 5-wire trunk cable |
| Speed | 4800-38400 baud | 250 kbps |
| Data Format | ASCII sentences | Binary PGN messages |
| Devices | 1 talker, multiple listeners | Up to 50 devices |
| Power | Separate | Power over network (12V) |
| Common Use | GPS, AIS, older instruments | Modern marine electronics |

### Which Devices Use NMEA 2000?

**NMEA 2000 Devices (Use CAN Terminal Blocks):**
- Modern chartplotters (Garmin, Raymarine, Simrad, Lowrance)
- Engine monitoring systems
- Fuel management systems
- Tank level sensors
- Battery monitors
- Digital switching systems
- Advanced autopilots
- Multi-function displays (MFDs)
- Weather stations
- Radar systems

**NMEA 0183 Devices (Use RS485 or GPIO):**
- Most GPS modules
- Simple depth sounders
- Older wind instruments
- Basic AIS receivers
- Legacy marine electronics

## 🎛️ T-CAN485 CAN Bus Terminals

Your T-CAN485 board has **two green terminal blocks**. The **LEFT block** is for NMEA 2000 (CAN bus):

```
┌─────────────────────────────────────┐
│      LILYGO T-CAN485 Board          │
│                                     │
│      [USB-C Port]                   │
│                                     │
│      [ESP32 Chip]                   │
│                                     │
│   ┌───────┐       ┌───────┐        │
│   │ CAN   │       │ RS485 │        │  ← Green Terminal Blocks
│   │ H   L │       │ A   B │        │
│   └───────┘       └───────┘        │
│    LEFT            RIGHT            │
│   (NMEA 2000)    (NMEA 0183)       │
│                                     │
└─────────────────────────────────────┘

CAN Terminal Block (LEFT):
├─ H (CANH) → NMEA 2000 High (WHITE wire)
└─ L (CANL) → NMEA 2000 Low  (BLUE wire)
```

## 🔌 NMEA 2000 Network Basics

### Standard NMEA 2000 Cable Colors

NMEA 2000 uses a **5-wire system** with standard colors:

| Wire Color | Signal | Function | Connect To |
|------------|--------|----------|------------|
| **WHITE** | CAN_H | Data High | **H** terminal on T-CAN485 |
| **BLUE** | CAN_L | Data Low | **L** terminal on T-CAN485 |
| **RED** | NET-S | Power (+12V) | 12V power supply |
| **BLACK** | NET-C | Ground (0V) | Common ground |
| **SHIELD** | SHIELD | Drain wire | Ground at ONE point only |

### Network Topology

NMEA 2000 uses a **trunk/drop** topology:

```
Terminator                                              Terminator
  120Ω                                                    120Ω
   │                                                        │
   ├────────────── Backbone Cable ─────────────────────────┤
   │                    │              │                    │
   │                  Drop          Drop                  Drop
   │                 Cable         Cable                 Cable
   │                    │              │                    │
   │                    ▼              ▼                    ▼
   │              [Chartplotter]  [Wind Sensor]     [T-CAN485]
   │
```

**Key Points:**
- **Backbone (Trunk)**: Main cable running through the boat (max 100m for small networks)
- **Drop Cables**: Short cables (max 6m each) connecting devices via T-connectors
- **Terminators**: 120Ω resistors at **both ends** of the backbone
- **Power**: 12V distributed through the backbone cable

## ⚙️ Connection Methods

### Option A: Direct Connection (Testing/Simple Setup)

If you only have one or two NMEA 2000 devices, connect directly:

```
NMEA 2000 Device (with bare wires or M12 connector)
│
├─ WHITE (CAN_H)  ────────→  H │ T-CAN485
├─ BLUE (CAN_L)   ────────→  L │ CAN Terminal
├─ RED (12V)      ────────→  12V Input Terminal
└─ BLACK (GND)    ────────→  GND

Add 120Ω resistor between H and L terminals
```

**When to Use:**
- Testing a single device
- Bench testing before installation
- Simple 2-device setup

### Option B: T-Connector to Existing Network (Recommended)

Connect to an existing NMEA 2000 backbone using a T-connector (drop cable):

```
              Existing NMEA 2000 Backbone
                         │
                         │
                    ┌────┴────┐
                    │ T-Conn  │  ← NMEA 2000 T-Connector
                    └────┬────┘     (Female M12 5-pin)
                         │
                    Drop Cable
                    (Max 6 meters)
                         │
                  ┌──────┴──────┐
                  │             │
        WHITE ────┤ H           │
         BLUE ────┤ L           │  T-CAN485 Board
          RED ────┤ 12V         │  (Optional - if powering from network)
        BLACK ────┤ GND         │
                  └─────────────┘
```

**When to Use:**
- Adding to existing boat NMEA 2000 network
- Production installation
- When other N2K devices already installed

### Option C: USB Power Only (Listen-Only)

For development/testing without connecting to boat power:

```
NMEA 2000 Backbone
├─ WHITE (CAN_H) ────→ H │ T-CAN485
├─ BLUE (CAN_L)  ────→ L │
└─ BLACK (GND)   ────→ GND (Important! Common ground required)

T-CAN485 powered via USB-C (5V from computer)
```

**Important:** Even with USB power, you **MUST** connect the ground wire from the NMEA 2000 network to the T-CAN485 ground. Without common ground, the CAN signals won't work.

## 🔧 Step-by-Step Installation

### Step 1: Plan Your Connection

**Identify your setup:**
- [ ] Are you connecting to an existing NMEA 2000 network?
- [ ] Will you use 12V boat power or USB power?
- [ ] Is the T-CAN485 at the end of the network (needs termination)?
- [ ] Do you need drop cables or direct connection?

### Step 2: Prepare the Wiring

**If using NMEA 2000 M12 connectors:**
1. Purchase a T-connector (female M12 5-pin)
2. Purchase a drop cable or make your own
3. Strip wires and identify colors

**If making custom cables:**
1. Use **twisted pair** for CAN_H and CAN_L (critical!)
2. Use separate wires for power and ground
3. Recommended: Use shielded cable
4. Wire gauge: 22-26 AWG for data, 18 AWG for power

### Step 3: Connect to T-CAN485

```
Wiring Sequence:

1. Power OFF - Disconnect all power sources first!

2. Connect CAN data lines:
   NMEA 2000 WHITE ───→ H terminal (left block)
   NMEA 2000 BLUE  ───→ L terminal (left block)

3. Connect power (if using):
   NMEA 2000 RED   ───→ 12V power input terminal
   NMEA 2000 BLACK ───→ GND power input terminal

4. If at end of network:
   Solder 120Ω resistor between H and L terminals
   (Or use NMEA 2000 terminator plug)

5. Secure connections:
   Tighten terminal block screws firmly
   Use wire ferrules for best connection
```

### Step 4: Verify Installation

**Before powering on:**
- [ ] All connections tight and secure
- [ ] No shorts between H and L
- [ ] Correct polarity on power connections
- [ ] Termination resistors installed (both ends of network)

**Measure with Multimeter:**
```
Power OFF, network disconnected:
- H to L: Should read ~60Ω (two 120Ω terminators in series)
- H to GND: Should read high impedance (no short)
- L to GND: Should read high impedance (no short)

Power ON:
- 12V terminal to GND: Should read 11-14V
- H to GND: Should read ~2.5V (idle)
- L to GND: Should read ~2.5V (idle)
- H to L: Should read ~0V (differential idle)
```

### Step 5: Configure and Test

The firmware is **already configured** for NMEA 2000. No changes needed!

**Upload firmware (if not already done):**
```bash
pio run --target upload
```

**Monitor serial output:**
```bash
pio device monitor --baud 115200
```

**You should see:**
```
Initializing NMEA2000...
CAN device ready
NMEA2000 initialized successfully
```

**When NMEA 2000 messages are received:**
```
N2K: Position received - Lat: XX.XXXXX Lon: YY.YYYYY
N2K: COG: XXX.X° SOG: XX.X kn
N2K: Wind Speed: XX.X kn Angle: XXX°
N2K: Water Depth: XX.X meters
N2K: Temperature: XX.X°C Pressure: XXXX Pa
```

## 📊 Supported NMEA 2000 Messages (PGNs)

Your firmware currently supports these Parameter Group Numbers:

| PGN | Name | Data Provided | SignalK Path |
|-----|------|---------------|--------------|
| 129025 | Position, Rapid Update | Latitude, Longitude | navigation.position |
| 129026 | COG & SOG, Rapid Update | Course, Speed | navigation.courseOverGroundTrue, navigation.speedOverGround |
| 130306 | Wind Data | Wind Speed, Angle | environment.wind.speedApparent, angleApparent |
| 128267 | Water Depth | Depth, Offset | environment.depth.belowTransducer |
| 130310 | Environmental Parameters | Temperature, Pressure, Humidity | environment.outside.temperature, pressure |

## 🔍 Testing Your Connection

### Test 1: Visual Inspection

**Check:**
- [ ] Wires securely fastened in terminal blocks
- [ ] No exposed wire touching between H and L
- [ ] Termination resistor installed (if at network end)
- [ ] Power connections correct polarity

### Test 2: Multimeter Measurements

**With network powered:**
```bash
Expected values:
- H terminal to GND: 2.0-3.0V DC
- L terminal to GND: 2.0-3.0V DC
- H to L differential: -0.5V to +0.5V (idle)

During transmission (fluctuating):
- H to L differential: ±1.5V to ±3V
```

**If readings are wrong:**
- Both H and L near 0V: Check termination (too low resistance)
- Both H and L near 5V: Missing termination or open circuit
- H and L more than 1V apart (idle): Possible short or bad terminator

### Test 3: Serial Monitor Check

**Monitor for CAN bus errors:**
```bash
pio device monitor --baud 115200 | grep -i "can\|n2k\|error"
```

**Good output:**
```
CAN device ready
NMEA2000 initialized successfully
N2K: Position received
```

**Problem output:**
```
CAN: Bus error
CAN: No ACK received
NMEA2000 initialization failed
```

### Test 4: SignalK API Check

**Access the SignalK API from your browser:**
```
http://172.20.10.2:3000/signalk/v1/api/vessels/self
```

**Look for NMEA 2000 data:**
```json
{
  "navigation": {
    "position": {
      "value": {
        "latitude": 59.12345,
        "longitude": 18.67890
      },
      "timestamp": "2025-11-12T14:30:00.000Z",
      "$source": "nmea2000.25"
    }
  }
}
```

## 🐛 Troubleshooting

### Problem: No NMEA 2000 Data Received

**Possible Causes:**

1. **Wiring Issues**
   - [ ] Verify WHITE to H, BLUE to L (not swapped)
   - [ ] Check for loose connections in terminal blocks
   - [ ] Ensure twisted pair used for CAN_H and CAN_L
   - [ ] Look for damaged/cut wires

2. **Power Issues**
   - [ ] Verify 12V present on network (measure RED wire)
   - [ ] Check ground connection (BLACK wire to GND)
   - [ ] If using USB power only, verify GND wire connected
   - [ ] NMEA 2000 devices need power to transmit

3. **Termination Issues**
   - [ ] Network needs 120Ω resistor at BOTH ends
   - [ ] Measure H-to-L resistance (should be ~60Ω with power off)
   - [ ] Too many terminators will cause bus failure
   - [ ] Missing terminators will cause reflections/errors

4. **Network Configuration**
   - [ ] Check if NMEA 2000 devices are powered on
   - [ ] Verify devices are actually transmitting (check on chartplotter)
   - [ ] Some devices need configuration to enable NMEA 2000 output

### Problem: CAN Bus Errors in Serial Monitor

**Error Messages and Solutions:**

**"CAN: Bus-off state"**
- Too many errors, bus has shut down
- Check termination resistors
- Verify no shorts between H and L
- Check baud rate is 250 kbps (firmware default)

**"CAN: No ACK received"**
- No other devices on the network responding
- T-CAN485 might be the only device (normal if true)
- In listen-only mode, this is expected

**"CAN: Bit error"**
- Signal integrity issue
- Check twisted pair wiring for CAN_H/CAN_L
- Reduce drop cable length (max 6m)
- Add/check termination resistors

**"CAN: CRC error"**
- Data corruption
- Check for electrical noise
- Verify shield/ground connections
- Move CAN cables away from power cables

### Problem: Intermittent Data

**Solutions:**

1. **Check Cable Quality**
   - [ ] Use twisted pair for CAN_H and CAN_L
   - [ ] Keep drop cables under 6 meters
   - [ ] Use quality connectors (M12 standard)
   - [ ] Avoid running near high-current wires

2. **Verify Termination**
   - [ ] Exactly TWO 120Ω resistors on network (one at each end)
   - [ ] Measure 60Ω between H and L (power off, devices disconnected)
   - [ ] Don't use resistors of wrong value

3. **Environmental Issues**
   - [ ] Check for water ingress in connectors
   - [ ] Verify connections not corroded
   - [ ] Protect exposed wires from moisture
   - [ ] Use dielectric grease in marine environments

### Problem: T-CAN485 Resets When Connected

**Possible Causes:**

1. **Power Supply Issues**
   - NMEA 2000 network voltage too low (<10V)
   - Voltage spikes from engine starting
   - USB power insufficient

2. **Solutions:**
   - [ ] Use 12V boat power instead of USB
   - [ ] Add filtering capacitor (1000µF) on 12V input
   - [ ] Check network voltage under load
   - [ ] Use separate power supply for T-CAN485

## 📏 Cable Specifications

### NMEA 2000 Certified Cable

**Backbone (Trunk) Cable:**
- Type: NMEA 2000 certified backbone cable
- Connectors: M12 5-pin circular
- Colors: Standard (White, Blue, Red, Black, Shield)
- Max length: 100m (328ft) for small networks
- Max length: 200m (656ft) with proper design

**Drop Cables:**
- Type: NMEA 2000 certified drop cable
- Max length: 6 meters (20 feet) each
- Same 5-wire configuration as backbone

**T-Connectors:**
- Type: M12 5-pin T-connector
- One inline (backbone continuation)
- One 90° (device connection)

### DIY Alternative Cable

If making your own cables:

| Component | Specification |
|-----------|---------------|
| Data Pair (H/L) | Twisted pair, 120Ω impedance, 22-26 AWG |
| Power (RED) | 18 AWG minimum for loads >1A |
| Ground (BLACK) | 18 AWG minimum |
| Shield | Foil or braid, grounded at one point |
| Overall | Shielded, marine-rated if possible |

**Recommended Cables:**
- Belden 3084A (NMEA 2000 DeviceNet cable)
- Alpha Wire 6452
- Any CAN bus cable rated for 250 kbps

## 🔄 Adding More NMEA 2000 Devices

### Network Sizing

**Limits:**
- **Max devices**: 50 devices (theoretical), 30 devices (practical)
- **Max backbone**: 100-200 meters
- **Max drop cable**: 6 meters per device
- **Max current**: 3A total on network power (LEN = Load Equivalency Number)

### Device Installation Order

1. Start at one end of the boat (typically bow or stern)
2. Install backbone cable
3. Add T-connectors where devices will be located
4. Connect drop cables (keep as short as possible)
5. Connect T-CAN485 using a T-connector and drop cable
6. Add 120Ω terminator at each end of backbone

### Example: Adding T-CAN485 to Existing Network

**Scenario:** You have a Garmin chartplotter and wind sensor already on NMEA 2000.

**Steps:**
1. **Locate nearest T-connector** on existing backbone
2. **Purchase drop cable** (or make one, max 6m)
3. **Connect drop cable** to T-connector
4. **Wire to T-CAN485:**
   - WHITE → H terminal
   - BLUE → L terminal
   - RED → 12V power (optional)
   - BLACK → GND
5. **Do NOT add terminator** (network already terminated)
6. **Power on and test**

## 🛡️ Safety and Best Practices

### Electrical Safety

1. **Always power off before connecting/disconnecting**
   - NMEA 2000 devices can be damaged by hot-plugging
   - Prevent shorts when working with live wires

2. **Polarity Matters**
   - RED is always +12V
   - BLACK is always ground (0V)
   - Reversed polarity will damage devices!

3. **Fusing**
   - Add 3A fuse on RED (+12V) wire close to power source
   - Protects against shorts in cable

### Marine Environment

1. **Waterproofing**
   - Use marine-grade connectors
   - Apply dielectric grease to all connections
   - Seal terminal blocks with heat shrink or rubber boots
   - Mount T-CAN485 in dry location

2. **Corrosion Prevention**
   - Use tinned copper wire in marine environments
   - Apply corrosion inhibitor to terminals
   - Inspect connections annually

3. **Strain Relief**
   - Secure cables with cable ties
   - Avoid sharp bends (min 25mm radius)
   - Use cable glands where passing through bulkheads

### Installation Quality

1. **Use Proper Tools**
   - Wire stripper (not knife)
   - Ferrule crimper for terminal blocks
   - Multimeter for testing

2. **Follow Standards**
   - NMEA 2000 installation standard
   - ABYC E-11 (AC and DC Electrical Systems on Boats)
   - Keep documentation of network layout

3. **Label Everything**
   - Label both ends of each cable
   - Document network topology
   - Note device addresses/instances

## 📘 Advanced Topics

### Multiple T-CAN485 Devices on Same Network

You can have **multiple ESP32 devices** on the same NMEA 2000 network:

**Requirements:**
- Each device needs unique NMEA 2000 address (set in firmware)
- Each device needs unique instance numbers
- Total network current must stay under limit

**To set unique address in firmware:**
```cpp
// In src/main.cpp, find initNMEA2000() function
NMEA2000.SetDeviceInformation(
  1,    // Unique number (change for each device: 1, 2, 3...)
  130,  // Device function
  25,   // Device class
  2046  // Manufacturer code
);
```

### Transmitting Data (Not Just Receiving)

Your current firmware is in **listen-only mode**. To transmit data:

**Change in src/main.cpp:**
```cpp
// Find this line:
NMEA2000.SetMode(tNMEA2000::N2km_ListenOnly, 25);

// Change to:
NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, 25);
```

Then you can send NMEA 2000 messages from sensors connected to ESP32.

### Debugging with NMEA Reader

**NMEA Reader** (Windows software) can help diagnose:
- Install NMEA Reader on PC
- Connect T-CAN485 via USB
- Configure as NMEA 2000 gateway
- Monitor all PGNs on network in real-time

## 📞 Getting Help

If you're still having trouble:

1. **Check Serial Monitor** - Most issues show errors here
2. **Verify Voltages** - Use multimeter to check H, L, power
3. **Test Termination** - Measure 60Ω between H and L
4. **Isolate Problem** - Test with single device first
5. **Check Device Settings** - Some devices need N2K output enabled
6. **Consult Device Manual** - Verify device is transmitting expected PGNs

### Useful Resources

- NMEA 2000 Standard: https://www.nmea.org/nmea-2000.html
- T-CAN485 Hardware: https://github.com/Xinyuan-LilyGO/T-CAN485
- NMEA2000 Library: https://github.com/ttlappalainen/NMEA2000
- SignalK Documentation: https://signalk.org/

---

**Board**: LILYGO TTGO T-CAN485
**Firmware**: ESP32 SignalK Gateway
**CAN Transceiver**: SN65HVD231 (built-in)
**CAN Bus Pins**: GPIO 27 (TX), GPIO 26 (RX) - Hardware fixed
**Last Updated**: 2025
