# Troubleshooting Guide: Single-Ended NMEA 0183

This guide helps diagnose and fix common issues with Single-Ended NMEA 0183 input on ESP32 SignalK.

## Quick Diagnostic Flowchart

```
Is data being received?
│
├─ YES → Data is garbled/corrupted
│  │
│  ├─ Check baud rate
│  ├─ Check voltage divider values
│  └─ Add noise filtering capacitor
│
└─ NO → No data received
   │
   ├─ Check Serial Monitor output
   │  │
   │  ├─ Shows "NO DATA for 30+ seconds"
   │  │  │
   │  │  ├─ NMEA device powered? → NO → Power the device
   │  │  │                          YES ↓
   │  │  ├─ Voltage divider working? → NO → Fix divider
   │  │  │                             YES ↓
   │  │  ├─ GND connected? → NO → Connect GND
   │  │  │                   YES ↓
   │  │  └─ Device transmitting? → Test with USB-Serial
   │  │
   │  └─ No initialization message
   │     │
   │     ├─ USE_SINGLEENDED_NMEA defined? → NO → Enable in config.h
   │     └─ GPIO 33 available? → NO → Change pin
   │
   └─ Cannot access Serial Monitor → Check USB connection
```

## Problem Categories

### 1. No Data Received ⚠️

**Symptom:**
```
[Single-Ended NMEA] ⚠️ NO DATA for 30+ seconds
  Check: Voltage divider wiring, device power, baud rate
```

#### Solution A: Check NMEA Device Power

**Test:**
1. Verify power LED on NMEA device is ON
2. Measure voltage at device power terminals (should be 10-14V for 12V devices)
3. Check power supply capacity (minimum 500mA recommended)

**Fix:**
- Use proper marine power supply (12V DC)
- Check fuses in power line
- Ensure power connectors are tight

---

#### Solution B: Verify Voltage Divider Circuit

**Test with Multimeter:**

1. **Disconnect GPIO 33 from ESP32**
2. **Power ON the NMEA device**
3. **Set multimeter to DC voltage (20V range)**

**Measure these points:**

```
Test Point 1: NMEA OUT to GND
Expected: 0-12V (varying signal)
If reads: 0V constantly → Device not transmitting
If reads: 12V constantly → Device stuck HIGH
If reads: Correct range → Device is working ✓

Test Point 2: Junction (GPIO 33 point) to GND
Expected: 0-3.3V (varying signal)
If reads: 0V constantly → R1 disconnected or device not transmitting
If reads: Same as NMEA OUT → Voltage divider not working
If reads: >3.5V → WRONG RESISTORS! Do not connect to ESP32!
If reads: 0-3.3V → Voltage divider working ✓
```

**Common voltage divider problems:**

| Reading at GPIO 33 | Problem | Fix |
|-------------------|---------|-----|
| 0V | No input signal OR R1 disconnected | Check NMEA device, check R1 connection |
| 12V | R2 disconnected or missing | Install 3.9kΩ from junction to GND |
| 8.6V | R1 and R2 swapped! | Swap resistors: 10kΩ on top, 3.9kΩ on bottom |
| 6V | Wrong resistor values | Verify: R1=10kΩ, R2=3.9kΩ with multimeter |

**Fix:**
- Verify resistor values with multimeter (R1=10kΩ, R2=3.9kΩ)
- Check all solder joints
- Ensure no short circuits
- See [VOLTAGE_DIVIDER_CIRCUIT.md](VOLTAGE_DIVIDER_CIRCUIT.md) for proper wiring

---

#### Solution C: Check GND Connection

**Critical:** NMEA device GND MUST be connected to ESP32 GND!

**Test:**
1. Use multimeter in continuity mode
2. Check beep/continuity between:
   - NMEA device GND terminal
   - ESP32 GND pin

**If no continuity:**
- Add GND wire between NMEA device and ESP32
- Check for broken GND wire
- Ensure screw terminals are tight

---

#### Solution D: Verify Baud Rate

**Most NMEA devices use 4800 baud**, but some use 9600 or 38400.

**Test different baud rates:**

1. **Via Web Interface:**
   - Go to Hardware Settings
   - Try: 4800, 9600, 38400
   - Save and restart after each change

2. **Via config.h:**
   ```cpp
   #define SINGLEENDED_NMEA_BAUD 4800  // Try 9600 or 38400
   ```

3. **Check device manual** for correct baud rate

**Common baud rates by device type:**
- NASA wind instruments: **4800**
- Basic GPS modules: **4800** or **9600**
- AIS receivers: **38400**
- Modern depth sounders: **4800**

---

#### Solution E: Test NMEA Device Independently

Use a USB-Serial adapter to verify device is transmitting:

**Required hardware:**
- USB to TTL Serial adapter (3.3V or 5V)
- Computer with terminal software

**Steps:**

1. **Build test voltage divider:**
   ```
   NMEA OUT ─[10kΩ]─┬─ USB-Serial RX
                     │
                  [3.9kΩ]
                     │
                    GND
   ```

2. **Connect GND:** NMEA device GND to USB-Serial GND

3. **Open terminal software:**
   - Windows: PuTTY, Tera Term
   - Mac/Linux: screen, minicom
   - Arduino IDE Serial Monitor

4. **Configure terminal:**
   - Baud: 4800 (or try 9600, 38400)
   - Data bits: 8
   - Parity: None
   - Stop bits: 1

5. **Expected output:**
   ```
   $IIMWV,045,R,12.5,N,A*2E
   $IIMWV,046,R,12.3,N,A*2D
   $IIMWV,047,R,12.8,N,A*23
   ```

**If you see garbled text:**
- Wrong baud rate → Try different rates
- Voltage too high → Check voltage divider

**If you see nothing:**
- Device not powered
- Device not transmitting
- Wrong TX/RX connection
- Faulty device

---

### 2. Garbled/Corrupted Data 📟

**Symptom:**
```
Single-Ended RX: $��MW�,0���R,1��5,N,A*��
Single-Ended RX: $IIMWV,045,R,12.���������
```

#### Solution A: Wrong Baud Rate

Most common cause of garbled data!

**Fix:** Try different baud rates (4800, 9600, 38400)

**Quick test:** If you see **some** recognizable characters, baud rate is close:
- Try adjacent rates: 4800 ↔ 9600
- Check device manual for exact baud rate

---

#### Solution B: Voltage Levels Incorrect

**Test voltage at GPIO 33:**
- Should be: 0-3.3V
- If higher: Voltage divider wrong
- If lower than 0.8V: Signal too weak

**Fix:**
- Verify resistor values: R1=10kΩ, R2=3.9kΩ
- Check for cold solder joints
- Ensure good connections

---

#### Solution C: Electrical Noise

Marine environments have lots of electrical noise!

**Symptoms:**
- Occasional garbled characters
- Data works sometimes, fails other times
- Worse when engine/motors running

**Fix: Add noise filtering capacitor**

```
Add 0.1µF ceramic capacitor between GPIO 33 and GND:

NMEA OUT ─[10kΩ]─┬─ GPIO 33
                  │
               [3.9kΩ]
                  │   ┌─────┐
                  ├───│ 0.1µF│─── (capacitor)
                  │   └─────┘
                 GND
```

**Additional noise reduction:**
- Keep NMEA wires away from power cables
- Use shielded cable for NMEA signal
- Add ferrite beads on NMEA cable
- Ensure good GND connection

---

#### Solution D: SoftwareSerial Limitations

SoftwareSerial on ESP32 has limitations:

**Max reliable baud:** 38400
**Known issues:**
- WiFi activity can cause missed bytes
- High interrupt load affects reception

**Fix:**
- Use lower baud rate (4800 is most reliable)
- Disable WiFi temporarily for testing
- Consider hardware UART if available (requires code modification)

---

### 3. Intermittent Data Loss 📡

**Symptom:**
```
[Single-Ended NMEA] Received 120 bytes in last 10s
[Single-Ended NMEA] Received 0 bytes in last 10s      ← Lost data
[Single-Ended NMEA] Received 95 bytes in last 10s
[Single-Ended NMEA] ⚠️ NO DATA for 30+ seconds        ← Complete loss
[Single-Ended NMEA] Received 108 bytes in last 10s    ← Recovered
```

#### Solution A: Loose Connection

**Check:**
- All wire connections are secure
- Screw terminals are tight
- Solder joints are solid
- No corrosion on connectors

**Fix:**
- Re-solder all connections
- Use heat-shrink tubing
- Apply dielectric grease to connectors
- Secure wires with cable ties

---

#### Solution B: Power Supply Issues

**Symptoms:**
- Data stops when other equipment turns on
- Data loss correlates with voltage drops
- LED on NMEA device dims occasionally

**Test:**
- Measure voltage at NMEA device power terminals
- Should be stable 10-14V DC
- Check during high load (engine start, etc.)

**Fix:**
- Use dedicated power supply for NMEA device
- Add voltage regulator if supply is unstable
- Check power cable gauge (use heavier wire)
- Install capacitor on power line (470µF, 25V)

---

#### Solution C: ESP32 WiFi Interference

ESP32 WiFi can interfere with SoftwareSerial:

**Test:**
- Temporarily disable WiFi
- Check if data reception improves

**Fix in code (advanced):**
```cpp
// In setup(), temporarily disable WiFi:
WiFi.mode(WIFI_OFF);

// Test if data is received reliably
// If yes, WiFi interference is the cause
```

**Solutions:**
- Use lower baud rate (4800 instead of 9600)
- Reduce WiFi activity
- Consider using NMEA TCP server (no WiFi during NMEA read)

---

### 4. Wrong GPIO Pin Issues 🔌

#### Solution A: GPIO 33 Not Working

**Possible causes:**
- GPIO 33 is damaged (previous overvoltage)
- GPIO 33 is used by another peripheral
- Board design conflict

**Fix: Use alternative GPIO:**

**Good alternatives:**
- **GPIO 32** (if Seatalk1 not used)
- **GPIO 35** (input-only, no pullup)
- **GPIO 34** (input-only, no pullup)
- **GPIO 36** (input-only, no pullup, ADC1_CH0)
- **GPIO 39** (input-only, no pullup, ADC1_CH3)

**How to change:**

1. **In config.h:**
   ```cpp
   #define SINGLEENDED_NMEA_RX 32  // Change to 32
   ```

2. **Or via Web Interface:**
   - Hardware Settings → Single-Ended NMEA → RX Pin
   - Enter new pin number
   - Save and restart

3. **Move voltage divider connection** to new GPIO pin

**⚠️ Avoid these pins:**
- GPIO 0, 2, 5, 12, 15 (boot strapping pins)
- GPIO 6-11 (connected to flash memory)
- GPIO 21, 22 (RS485)
- GPIO 25, 18 (GPS)
- GPIO 26, 27 (CAN bus)

---

### 5. Configuration Issues ⚙️

#### Solution A: Feature Not Enabled

**Symptom:** No initialization message in Serial Monitor

**Check config.h:**
```cpp
#define USE_SINGLEENDED_NMEA  // Must be defined!
```

**If commented out:**
```cpp
// #define USE_SINGLEENDED_NMEA  // Currently disabled
```

**Fix:**
- Remove `//` to uncomment
- Rebuild and upload firmware

---

#### Solution B: Settings Not Saved

**Symptom:** Settings revert after restart

**Cause:** Web interface saves to flash, but restart needed

**Fix:**
1. Change settings in Hardware Settings page
2. Click "Save Configuration"
3. **Restart ESP32** (power cycle or press reset button)
4. Verify settings in Serial Monitor on boot

---

### 6. Advanced Diagnostics 🔬

#### Check 1: Monitor Raw Serial Data

Add debug output to see raw bytes:

**In main.cpp loop(), add before `handleNmeaSentence()`:**

```cpp
if (c >= 32 && c <= 126) {
  Serial.printf("RX byte: 0x%02X '%c'\n", c, c);  // Debug output
}
```

**Expected output:**
```
RX byte: 0x24 '$'
RX byte: 0x49 'I'
RX byte: 0x49 'I'
RX byte: 0x4D 'M'
RX byte: 0x57 'W'
RX byte: 0x56 'V'
...
```

**If you see:**
- No bytes → Device not transmitting or voltage divider broken
- Random bytes → Wrong baud rate or noise
- Some correct, some wrong → Intermittent connection

---

#### Check 2: Verify NMEA Checksum

All NMEA sentences have checksums:

```
$IIMWV,045,R,12.5,N,A*2E
                      ^^── Checksum (must match calculated value)
```

**If checksums fail:**
- Noise corruption
- Wrong baud rate
- Faulty NMEA device

**Enable checksum validation** (done automatically in firmware)

---

#### Check 3: Oscilloscope Analysis (Advanced)

If you have an oscilloscope:

**Probe GPIO 33:**
- Should see 0-3.3V square waves
- Baud rate determines timing
- Clean edges = good signal

**If you see:**
- No signal → Voltage divider broken
- Signal >3.3V → DO NOT CONNECT! Check divider
- Noisy/jittery signal → Add capacitor filter
- Sloped edges → Cable too long or capacitance issue

---

## Quick Reference: Error Messages

| Error Message | Meaning | First Steps |
|--------------|---------|-------------|
| `⚠️ NO DATA for 30+ seconds` | No NMEA data received | Check power, voltage divider, GND |
| `Failed to load settings` | Web UI issue | Refresh page, check network |
| `Initialization failed` | GPIO conflict | Try different GPIO pin |
| Garbled characters | Wrong baud rate | Try 4800, 9600, 38400 |
| Nothing in Serial Monitor | Config issue | Check `USE_SINGLEENDED_NMEA` |

## Testing Checklist

Before asking for help, verify:

- [ ] NMEA device is powered and LED is ON
- [ ] Voltage divider is correctly built (10kΩ + 3.9kΩ)
- [ ] Voltage at GPIO 33 is 0-3.3V (measured with multimeter)
- [ ] NMEA device GND is connected to ESP32 GND
- [ ] Correct baud rate configured (usually 4800)
- [ ] GPIO 33 connection is secure
- [ ] Tested with USB-Serial adapter (device transmits data)
- [ ] Firmware has `USE_SINGLEENDED_NMEA` defined
- [ ] Serial Monitor shows initialization message
- [ ] Tried different baud rates
- [ ] Added noise filtering capacitor (0.1µF)

## Common Device-Specific Issues

### NASA Wireless Wind

**Problem:** No data from NASA base unit

**Solutions:**
1. Verify base unit has power (LED should be ON)
2. Check wireless link to sensor (may need re-pairing)
3. Confirm baud rate is 4800
4. NASA outputs 0-12V → voltage divider required

---

### Generic GPS Modules

**Problem:** GPS works on USB but not on ESP32

**Solutions:**
1. Most GPS default to 9600 baud (not 4800)
2. Some GPS need configuration commands to enable NMEA output
3. Check GPS LED indicates fix (blinking = searching, solid = fix)

---

### Depth Sounders

**Problem:** Depth readings intermittent

**Solutions:**
1. Transducer may need cleaning (marine growth)
2. Check transducer cable for damage
3. Depth sounders often use 4800 baud
4. Some output DBT sentences, others DPT

---

## When All Else Fails

If you've tried everything:

1. **Post on support forum** with:
   - Serial Monitor output
   - Voltage measurements
   - Photos of your wiring
   - NMEA device model and manual

2. **Try a known-good NMEA device**
   - Borrow another device to test
   - Isolate if problem is device or ESP32

3. **Consider RS485 alternative**
   - If your device supports RS485 (differential)
   - No voltage divider needed
   - More reliable over long distances

4. **Check for hardware damage**
   - If GPIO 33 was previously exposed to >3.3V
   - May need to replace ESP32 module

## Support Resources

- **Documentation:** [SINGLE_ENDED_NMEA.md](SINGLE_ENDED_NMEA.md)
- **Circuit Guide:** [VOLTAGE_DIVIDER_CIRCUIT.md](VOLTAGE_DIVIDER_CIRCUIT.md)
- **NMEA 0183 Standard:** https://www.nmea.org/
- **ESP32 Forums:** https://esp32.com/
- **SignalK Community:** https://github.com/SignalK/signalk-server

---

**Remember:** Most NMEA issues are caused by:
1. Wrong baud rate (try 4800 first!)
2. Missing/incorrect voltage divider
3. No GND connection between devices

**Document Version:** 1.0
**Last Updated:** 2025-01-21
